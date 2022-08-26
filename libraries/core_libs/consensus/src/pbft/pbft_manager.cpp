/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "pbft/pbft_manager.hpp"

#include <libdevcore/SHA3.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "dag/dag.hpp"
#include "final_chain/final_chain.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/period_data.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {
using vrf_output_t = vrf_wrapper::vrf_output_t;

PbftManager::PbftManager(const PbftConfig &conf, const blk_hash_t &dag_genesis_block_hash, addr_t node_addr,
                         std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
                         std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                         std::shared_ptr<FinalChain> final_chain, std::shared_ptr<KeyManager> key_manager,
                         secret_t node_sk, vrf_sk_t vrf_sk, uint32_t max_levels_per_period)
    : db_(std::move(db)),
      next_votes_manager_(std::move(next_votes_mgr)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      final_chain_(std::move(final_chain)),
      key_manager_(std::move(key_manager)),
      node_addr_(std::move(node_addr)),
      node_sk_(std::move(node_sk)),
      vrf_sk_(std::move(vrf_sk)),
      LAMBDA_ms_MIN(conf.lambda_ms_min),
      COMMITTEE_SIZE(conf.committee_size),
      NUMBER_OF_PROPOSERS(conf.number_of_proposers),
      DAG_BLOCKS_SIZE(conf.dag_blocks_size),
      GHOST_PATH_MOVE_BACK(conf.ghost_path_move_back),
      RUN_COUNT_VOTES(false),  // this field is for tests only and almost the time is disabled
      dag_genesis_block_hash_(dag_genesis_block_hash),
      config_(conf),
      max_levels_per_period_(max_levels_per_period) {
  LOG_OBJECTS_CREATE("PBFT_MGR");
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  daemon_ = std::make_unique<std::thread>([this]() { run(); });
  LOG(log_dg_) << "PBFT daemon initiated ...";
  if (RUN_COUNT_VOTES) {
    monitor_stop_ = false;
    monitor_votes_ = std::make_shared<std::thread>([this]() { countVotes_(); });
    LOG(log_nf_test_) << "PBFT monitor vote logs initiated";
  }
}

void PbftManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  if (RUN_COUNT_VOTES) {
    monitor_stop_ = true;
    monitor_votes_->join();
    LOG(log_nf_test_) << "PBFT monitor vote logs terminated";
  }
  {
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.notify_all();
  }
  daemon_->join();
  final_chain_->stop();

  LOG(log_dg_) << "PBFT daemon terminated ...";
}

/* When a node starts up it has to sync to the current phase (type of block
 * being generated) and step (within the block generation round)
 * Five step loop for block generation over three phases of blocks
 * User's credential, sigma_i_p for a round p is sig_i(R, p)
 * Leader l_i_p = min ( H(sig_j(R,p) ) over set of j in S_i where S_i is set of
 * users from which have received valid round p credentials
 */
void PbftManager::run() {
  LOG(log_nf_) << "PBFT running ...";

  for (auto period = final_chain_->last_block_number() + 1, curr_period = pbft_chain_->getPbftChainSize();
       period <= curr_period; ++period) {
    auto period_raw = db_->getPeriodDataRaw(period);
    if (period_raw.size() == 0) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block in period " << period << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    PeriodData period_data(period_raw);
    if (period_data.pbft_blk->getPeriod() != period) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << period_data.pbft_blk->getBlockHash()
                   << " has different period " << period_data.pbft_blk->getPeriod()
                   << " in block data than in block order db: " << period;
      assert(false);
    }
    // We need this section because votes need to be verified for reward distribution
    for (const auto &v : period_data.previous_block_cert_votes) {
      validateVote(v);
    }
    finalize_(std::move(period_data), db_->getFinalizedDagBlockHashesByPeriod(period), period == curr_period);
  }
  vote_mgr_->replaceRewardVotes(db_->getLastBlockCertVotes());
  // Initialize PBFT status
  initialState();

  continuousOperation_();
}

// Only to be used for tests...
void PbftManager::resume() {
  // Will only appear in testing...
  LOG(log_si_) << "Resuming PBFT daemon...";

  if (step_ == 1) {
    state_ = value_proposal_state;
  } else if (step_ == 2) {
    state_ = filter_state;
  } else if (step_ == 3) {
    state_ = certify_state;
  } else if (step_ % 2 == 0) {
    state_ = finish_state;
  } else {
    state_ = finish_polling_state;
  }

  daemon_ = std::make_unique<std::thread>([this]() { continuousOperation_(); });
}

// Only to be used for tests...
void PbftManager::resumeSingleState() {
  if (!stopped_.load()) daemon_->join();
  stopped_ = false;

  if (step_ == 1) {
    state_ = value_proposal_state;
  } else if (step_ == 2) {
    state_ = filter_state;
  } else if (step_ == 3) {
    state_ = certify_state;
  } else if (step_ % 2 == 0) {
    state_ = finish_state;
  } else {
    state_ = finish_polling_state;
  }

  doNextState_();
}

// Only to be used for tests...
void PbftManager::doNextState_() {
  auto initial_state = state_;

  while (!stopped_ && state_ == initial_state) {
    if (stateOperations_()) {
      continue;
    }

    // PBFT states
    switch (state_) {
      case value_proposal_state:
        proposeBlock_();
        break;
      case filter_state:
        identifyBlock_();
        break;
      case certify_state:
        certifyBlock_();
        break;
      case finish_state:
        firstFinish_();
        break;
      case finish_polling_state:
        secondFinish_();
        break;
      default:
        LOG(log_er_) << "Unknown PBFT state " << state_;
        assert(false);
    }

    setNextState_();
    if (state_ != initial_state) {
      return;
    }
    sleep_();
  }
}

void PbftManager::continuousOperation_() {
  while (!stopped_) {
    if (stateOperations_()) {
      continue;
    }

    // PBFT states
    switch (state_) {
      case value_proposal_state:
        proposeBlock_();
        break;
      case filter_state:
        identifyBlock_();
        break;
      case certify_state:
        certifyBlock_();
        break;
      case finish_state:
        firstFinish_();
        break;
      case finish_polling_state:
        secondFinish_();
        break;
      default:
        LOG(log_er_) << "Unknown PBFT state " << state_;
        assert(false);
    }

    setNextState_();
    sleep_();
  }
}

std::pair<bool, uint64_t> PbftManager::getDagBlockPeriod(blk_hash_t const &hash) {
  std::pair<bool, uint64_t> res;
  auto value = db_->getDagBlockPeriod(hash);
  if (value == nullptr) {
    res.first = false;
  } else {
    res.first = true;
    res.second = value->first;
  }
  return res;
}

uint64_t PbftManager::getPbftPeriod() const { return pbft_chain_->getPbftChainSize() + 1; }

uint64_t PbftManager::getPbftRound() const { return round_; }

std::pair<uint64_t, uint64_t> PbftManager::getPbftRoundAndPeriod() const { return {getPbftRound(), getPbftPeriod()}; }

uint64_t PbftManager::getPbftStep() const { return step_; }

void PbftManager::setPbftRound(uint64_t const round) {
  db_->savePbftMgrField(PbftMgrRoundStep::PbftRound, round);
  round_ = round;
}

size_t PbftManager::getSortitionThreshold() const { return sortition_threshold_; }

size_t PbftManager::getTwoTPlusOne() const { return two_t_plus_one_; }

// Notice: Test purpose
void PbftManager::setSortitionThreshold(size_t const sortition_threshold) {
  sortition_threshold_ = sortition_threshold;
}

void PbftManager::updateDposState_() {
  dpos_period_ = pbft_chain_->getPbftChainSize();
  do {
    try {
      dpos_votes_count_ = final_chain_->dpos_eligible_total_vote_count(dpos_period_);
      weighted_votes_count_ = final_chain_->dpos_eligible_vote_count(dpos_period_, node_addr_);
      break;
    } catch (state_api::ErrFutureBlock &c) {
      LOG(log_nf_) << c.what();
      LOG(log_nf_) << "PBFT period " << dpos_period_ << " is too far ahead of DPOS, need wait! PBFT chain size "
                   << pbft_chain_->getPbftChainSize() << ", have executed chain size "
                   << final_chain_->last_block_number();
      // Sleep one PBFT lambda time
      thisThreadSleepForMilliSeconds(POLLING_INTERVAL_ms);
    }
  } while (!stopped_);

  LOG(log_nf_) << "DPOS total votes count is " << dpos_votes_count_ << " for period " << dpos_period_ << ". Account "
               << node_addr_ << " has " << weighted_votes_count_ << " weighted votes";
}

size_t PbftManager::getDposTotalVotesCount() const { return dpos_votes_count_.load(); }

size_t PbftManager::getDposWeightedVotesCount() const { return weighted_votes_count_.load(); }

size_t PbftManager::dposEligibleVoteCount_(addr_t const &addr) {
  try {
    return final_chain_->dpos_eligible_vote_count(dpos_period_, addr);
  } catch (state_api::ErrFutureBlock &c) {
    LOG(log_er_) << c.what() << ". Period " << dpos_period_ << " is too far ahead of DPOS";
    return 0;
  }
}

void PbftManager::setPbftStep(size_t const pbft_step) {
  last_step_ = step_;
  db_->savePbftMgrField(PbftMgrRoundStep::PbftStep, pbft_step);
  step_ = pbft_step;

  if (step_ > MAX_STEPS && LAMBDA_backoff_multiple < 8) {
    // Note: We calculate the lambda for a step independently of prior steps
    //       in case missed earlier steps.
    std::uniform_int_distribution<u_long> distribution(0, step_ - MAX_STEPS);
    auto lambda_random_count = distribution(random_engine_);
    LAMBDA_backoff_multiple = 2 * LAMBDA_backoff_multiple;
    LAMBDA_ms = std::min(kMaxLambda, LAMBDA_ms_MIN * (LAMBDA_backoff_multiple + lambda_random_count));
    LOG(log_dg_) << "Surpassed max steps, exponentially backing off lambda to " << LAMBDA_ms << " ms in round "
                 << getPbftRound() << ", step " << step_;
  } else {
    LAMBDA_ms = LAMBDA_ms_MIN;
    LAMBDA_backoff_multiple = 1;
  }
}

void PbftManager::resetStep() {
  last_step_ = step_;
  step_ = 1;
  startingStepInRound_ = 1;

  LAMBDA_ms = LAMBDA_ms_MIN;
  LAMBDA_backoff_multiple = 1;
}

bool PbftManager::tryPushCertVotesBlock() {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();

  auto certified_block = vote_mgr_->getVotesBundle(current_pbft_round, current_pbft_period, 3, two_t_plus_one_);
  // Not enough cert votes found yet
  if (!certified_block.has_value()) {
    return false;
  }

  LOG(log_dg_) << "Found enough cert votes for PBFT block " << certified_block->voted_block_hash << ", round "
               << current_pbft_round << ", period " << current_pbft_period;

  // Puts pbft block into chain
  if (!pushCertVotedPbftBlockIntoChain_(certified_block->voted_block_hash, std::move(certified_block->votes))) {
    return false;
  }

  duration_ = std::chrono::system_clock::now() - now_;
  auto execute_trxs_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
  LOG(log_dg_) << "PBFT block " << certified_block->voted_block_hash << " certified and pushed into chain in round "
               << current_pbft_round << ". Execution time " << execute_trxs_in_ms << " [ms]";
  return true;
}

bool PbftManager::advancePeriod() {
  resetPbftConsensus(1 /* round */);

  // Move to a new period, cleanup previous period votes
  const auto new_period = pbft_chain_->getPbftChainSize() + 1;
  vote_mgr_->cleanupVotesByPeriod(new_period);
  LOG(log_nf_) << "Period advanced to: " << new_period << ", round reset to 1";

  // Restart while loop...
  return true;
}

bool PbftManager::advanceRound() {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();

  auto determined_round = vote_mgr_->determineRoundFromPeriodAndVotes(current_pbft_period, two_t_plus_one_);
  if (determined_round <= current_pbft_round) {
    return false;
  }

  LOG(log_nf_) << "Round advanced to: " << determined_round << ", period " << current_pbft_period;

  resetPbftConsensus(determined_round);

  // Move to a new round, cleanup previous round votes
  vote_mgr_->cleanupVotesByRound(current_pbft_period, determined_round);

  // Restart while loop...
  return true;
}

void PbftManager::resetPbftConsensus(uint64_t round) {
  LOG(log_dg_) << "Reset PBFT consensus to: round " << round << ", period " << getPbftPeriod()
               << ", step 1, and resetting clock.";
  round_clock_initial_datetime_ = now_;

  // Update current round and reset step to 1
  round_ = round;
  resetStep();
  state_ = value_proposal_state;

  auto batch = db_->createWriteBatch();

  // Update in DB first
  db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftRound, round, batch);
  db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftStep, 1, batch);

  db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold, sortition_threshold_,
                                     batch);
  db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, dpos_period_.load(), batch);
  db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount,
                                     getDposTotalVotesCount(), batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);

  // Reset cert voted block in the new upcoming round
  if (cert_voted_block_for_round_.has_value() &&
      cert_voted_block_for_round_->second <= pbft_chain_->getPbftChainSize()) {
    db_->removePbftMgrVotedValueToBatch(PbftMgrVotedValue::CertVotedBlockInRound, batch);

    cert_voted_block_for_round_.reset();
  }

  // Reset soft voted block in the new upcoming round
  if (soft_voted_block_for_round_.has_value()) {
    // Cleanup soft votes for previous round
    db_->removeSoftVotesToBatch(round, batch);
    db_->removePbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockInRound, batch);

    soft_voted_block_for_round_.reset();
  }
  db_->commitWriteBatch(batch);

  should_have_cert_voted_in_this_round_ = false;

  // reset next voted value since start a new round
  // these are used to prevent voting multiple times while polling through the step
  // under current implementation.
  // TODO: Get rid of this way of doing it!
  next_voted_null_block_hash_ = false;
  next_voted_soft_value_ = false;

  polling_state_print_log_ = true;

  if (executed_pbft_block_) {
    updateDposState_();
    // reset sortition_threshold and TWO_T_PLUS_ONE
    updateTwoTPlusOneAndThreshold_();
    db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
    executed_pbft_block_ = false;
  }

  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::sleep_() {
  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
  LOG(log_tr_) << "elapsed time in round(ms): " << elapsed_time_in_round_ms_ << ", step " << step_;
  // Add 25ms for practical reality that a thread will not stall for less than 10-25 ms...
  if (next_step_time_ms_ > elapsed_time_in_round_ms_ + 25) {
    auto time_to_sleep_for_ms = next_step_time_ms_ - elapsed_time_in_round_ms_;
    LOG(log_tr_) << "Time to sleep(ms): " << time_to_sleep_for_ms << " in round " << getPbftRound() << ", step "
                 << step_;
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(time_to_sleep_for_ms));
  } else {
    LOG(log_tr_) << "Skipping sleep, running late...";
  }
}

void PbftManager::initialState() {
  // Initial PBFT state

  // Time constants...
  LAMBDA_ms = LAMBDA_ms_MIN;

  auto round = db_->getPbftMgrField(PbftMgrRoundStep::PbftRound);
  auto step = db_->getPbftMgrField(PbftMgrRoundStep::PbftStep);

  if (round == 1 && step == 1) {
    // Node start from scratch
    state_ = value_proposal_state;
  } else if (step < 4) {
    // Node start from DB, skip step 1 or 2 or 3
    step = 4;
    state_ = finish_state;
  } else if (step % 2 == 0) {
    // Node start from DB in first finishing state
    state_ = finish_state;
  } else if (step % 2 == 1) {
    // Node start from DB in second finishing state
    state_ = finish_polling_state;
  } else {
    LOG(log_er_) << "Unexpected condition at round " << round << " step " << step;
    assert(false);
  }

  // This is used to offset endtime for second finishing step...
  startingStepInRound_ = step;
  setPbftStep(step);
  round_ = round;

  // Initial last sync request
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  auto soft_voted_block = db_->getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockInRound);
  if (soft_voted_block.has_value()) {
    // From DB
    soft_voted_block_for_round_ = soft_voted_block;
    for (const auto &vote : db_->getSoftVotes(round)) {
      if (vote->getBlockHash() != soft_voted_block->first) {
        LOG(log_er_) << "The soft vote should have voting value " << soft_voted_block->first << ". Vote" << vote;
        continue;
      }
      vote_mgr_->addVerifiedVote(vote);
    }
  } else {
    // Default value
    soft_voted_block_for_round_.reset();
  }

  if (auto cert_voted_block = db_->getPbftMgrVotedValue(PbftMgrVotedValue::CertVotedBlockInRound);
      cert_voted_block.has_value()) {
    cert_voted_block_for_round_ = *cert_voted_block;
    LOG(log_nf_) << "Initialize last cert voted value " << cert_voted_block_for_round_->first;
  }

  executed_pbft_block_ = db_->getPbftMgrStatus(PbftMgrStatus::ExecutedBlock);
  next_voted_soft_value_ = db_->getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue);
  next_voted_null_block_hash_ = db_->getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash);

  round_clock_initial_datetime_ = std::chrono::system_clock::now();
  current_step_clock_initial_datetime_ = round_clock_initial_datetime_;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  next_step_time_ms_ = 0;

  updateDposState_();
  // Initializetwo_t_plus_one_and sortition_threshold
  updateTwoTPlusOneAndThreshold_();

  if (round > 1) {
    // Get next votes for previous round from DB
    auto next_votes_in_previous_round = db_->getPreviousRoundNextVotes();
    if (next_votes_in_previous_round.empty()) {
      LOG(log_er_) << "Cannot get any next votes in previous round " << round - 1 << ". For period " << getPbftPeriod()
                   << " step " << step;
      assert(false);
    }

    // !!! Important: call only after updateTwoTPlusOneAndThreshold_()
    const auto two_t_plus_one = getTwoTPlusOne();
    next_votes_manager_->updateNextVotes(next_votes_in_previous_round, two_t_plus_one);
  }

  previous_round_next_voted_value_ = next_votes_manager_->getVotedValue();
  previous_round_next_voted_null_block_hash_ = next_votes_manager_->haveEnoughVotesForNullBlockHash();

  LOG(log_nf_) << "Node initialize at round " << round << ", period " << getPbftPeriod() << ", step " << step
               << ". Previous round has enough next votes for NULL_BLOCK_HASH: " << std::boolalpha
               << next_votes_manager_->haveEnoughVotesForNullBlockHash() << ", voted value "
               << previous_round_next_voted_value_ << ", next votes size in previous round is "
               << next_votes_manager_->getNextVotesWeight();
}

void PbftManager::setNextState_() {
  switch (state_) {
    case value_proposal_state:
      setFilterState_();
      break;
    case filter_state:
      setCertifyState_();
      break;
    case certify_state:
      if (go_finish_state_) {
        setFinishState_();
      } else {
        next_step_time_ms_ += POLLING_INTERVAL_ms;
      }
      break;
    case finish_state:
      setFinishPollingState_();
      break;
    case finish_polling_state:
      if (loop_back_finish_state_) {
        loopBackFinishState_();
      } else {
        next_step_time_ms_ += POLLING_INTERVAL_ms;
      }
      break;
    default:
      LOG(log_er_) << "Unknown PBFT state " << state_;
      assert(false);
  }
  LOG(log_tr_) << "next step time(ms): " << next_step_time_ms_ << ", step " << step_;
}

void PbftManager::setFilterState_() {
  state_ = filter_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * LAMBDA_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
  polling_state_print_log_ = true;
}

void PbftManager::setCertifyState_() {
  state_ = certify_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * LAMBDA_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
  polling_state_print_log_ = true;
}

void PbftManager::setFinishState_() {
  LOG(log_dg_) << "Will go to first finish State";
  state_ = finish_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 4 * LAMBDA_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
  polling_state_print_log_ = true;
}

void PbftManager::setFinishPollingState_() {
  state_ = finish_polling_state;
  setPbftStep(step_ + 1);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->commitWriteBatch(batch);
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
  polling_state_print_log_ = true;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::loopBackFinishState_() {
  auto round = getPbftRound();
  LOG(log_dg_) << "CONSENSUS debug round " << round << " , step " << step_
               << " | next_voted_soft_value_ = " << next_voted_soft_value_ << " soft_voted_value_for_round = "
               << (soft_voted_block_for_round_.has_value() ? soft_voted_block_for_round_->first.abridged() : "no value")
               << " next_voted_null_block_hash_ = " << next_voted_null_block_hash_ << " cert_voted_value_for_round = "
               << (cert_voted_block_for_round_.has_value() ? cert_voted_block_for_round_->first.abridged() : "no value")
               << " previous_round_next_voted_value_ = " << previous_round_next_voted_value_;
  state_ = finish_state;
  setPbftStep(step_ + 1);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->commitWriteBatch(batch);
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
  polling_state_print_log_ = true;
  assert(step_ >= startingStepInRound_);
  next_step_time_ms_ += POLLING_INTERVAL_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

bool PbftManager::stateOperations_() {
  pushSyncedPbftBlocksIntoChain();

  checkPreviousRoundNextVotedValueChange_();

  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();

  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_tr_) << "PBFT current round: " << round << ", period: " << period << ", step " << step_;

  // Check if these is already 2t+1 cert votes for some valid block, if so - push it into the chain
  if (tryPushCertVotesBlock()) {
    return true;
  }

  // 2t+1 next votes were seen
  if (advanceRound()) {
    return true;
  }

  return false;
}

std::optional<std::pair<blk_hash_t, uint64_t>> PbftManager::getSoftVotedBlockForThisRound_() {
  // Check current round cache
  if (soft_voted_block_for_round_.has_value()) {
    // Have enough soft votes for current round already
    return soft_voted_block_for_round_;
  }

  auto [round, period] = getPbftRoundAndPeriod();

  const auto voted_block_hash_with_soft_votes = vote_mgr_->getVotesBundle(round, period, 2, two_t_plus_one_);
  if (voted_block_hash_with_soft_votes.has_value()) {
    // Have enough soft votes for a voted value
    auto batch = db_->createWriteBatch();
    soft_voted_block_for_round_ = {std::make_pair(voted_block_hash_with_soft_votes->voted_block_hash,
                                                  voted_block_hash_with_soft_votes->votes_period)};

    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockInRound, *soft_voted_block_for_round_, batch);
    db_->addSoftVotesToBatch(round, voted_block_hash_with_soft_votes->votes, batch);
    db_->commitWriteBatch(batch);

    return soft_voted_block_for_round_;
  }

  return {};
}

void PbftManager::checkPreviousRoundNextVotedValueChange_() {
  previous_round_next_voted_value_ = next_votes_manager_->getVotedValue();
  previous_round_next_voted_null_block_hash_ = next_votes_manager_->haveEnoughVotesForNullBlockHash();
}

void PbftManager::proposeBlock_() {
  // Value Proposal
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_dg_) << "PBFT value proposal state in round: " << round << ", period: " << period;

  if (round == 1 || next_votes_manager_->haveEnoughVotesForNullBlockHash()) {
    if (round > 1) {
      LOG(log_nf_) << "Previous round " << round - 1 << " had next voted NULL_BLOCK_HASH";
    }

    proposed_block_ = proposePbftBlock_();

    if (proposed_block_) {
      if (auto vote = generateVoteWithWeight(proposed_block_->getBlockHash(), propose_vote_type, period, round, step_);
          vote) {
        if (auto net = network_.lock()) {
          LOG(log_nf_) << "Placed propose vote for new block " << proposed_block_->getBlockHash() << ", vote weight "
                       << *vote->getWeight() << ", period " << period << ", round " << round << ", step " << step_;

          // In propose step, send block before vote as we require block to be present during vote validation
          net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(proposed_block_);
          net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes({std::move(vote)});
        }
      }
    }
    return;
  } else if (previous_round_next_voted_value_.first) {
    // Round greater than 1 and next voted some value that is not null block hash

    LOG(log_nf_) << "Previous round " << round - 1 << " next voted block is " << previous_round_next_voted_value_.first;

    auto pbft_block = pbft_chain_->getUnverifiedPbftBlock(previous_round_next_voted_value_.first);
    if (!pbft_block) {
      LOG(log_dg_) << "Unable to find proposal block " << previous_round_next_voted_value_.first
                   << " from previous round in unverified queue";

      pbft_block = db_->getPbftCertVotedBlock(previous_round_next_voted_value_.first);
      if (!pbft_block) {
        LOG(log_dg_) << "Unable to find proposal block " << previous_round_next_voted_value_.first
                     << " from previous round in database";

        LOG(log_dg_) << "Unable to find proposal block " << previous_round_next_voted_value_.first
                     << " from previous round in either unverified queue or database, will not propose it";
        return;
      }
    }

    if (auto vote =
            generateVoteWithWeight(previous_round_next_voted_value_.first, propose_vote_type, period, round, step_);
        vote) {
      if (auto net = network_.lock()) {
        LOG(log_nf_) << "Placed propose vote for previous round next voted value "
                     << previous_round_next_voted_value_.first << ", vote weight " << *vote->getWeight() << ", period "
                     << period << ", round " << round << ", step " << step_;

        // In propose step, send block before vote as we require block to be present during vote validation
        net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
        net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes({std::move(vote)});
      }
    }
    return;
  } else {
    LOG(log_er_) << "Previous round " << round - 1 << " doesn't have enough next votes";
    assert(false);
  }
}

void PbftManager::identifyBlock_() {
  // The Filtering Step
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_dg_) << "PBFT filtering state in round: " << round << ", period: " << period;

  if (round == 1 || next_votes_manager_->haveEnoughVotesForNullBlockHash()) {
    // Identity leader
    if (auto leader_block = identifyLeaderBlock_(round, period); leader_block.has_value()) {
      auto [leader_block_hash, leader_block_period] = *leader_block;

      LOG(log_dg_) << "Leader block identified " << leader_block_hash << ", round " << round << ", period "
                   << leader_block_period;

      if (auto vote = generateVoteWithWeight(leader_block_hash, soft_vote_type, leader_block_period, round, step_);
          vote) {
        LOG(log_nf_) << "Placed soft vote for " << leader_block_hash << ", vote weight " << *vote->getWeight()
                     << ", round " << round << ", period " << leader_block_period << ", step " << step_;
        placeVote(std::move(vote));
      }
    }

    return;
  }

  // Soft vote for block from previous round
  if (!previous_round_next_voted_value_.first) {
    // There is no block from previous round, return
    return;
  }

  if (auto vote = generateVoteWithWeight(previous_round_next_voted_value_.first, soft_vote_type,
                                         previous_round_next_voted_value_.second, round, step_);
      vote) {
    LOG(log_nf_) << "Placed soft vote for block from previous round " << previous_round_next_voted_value_.first
                 << ", vote weight " << *vote->getWeight() << ", round " << round << ", period "
                 << previous_round_next_voted_value_.second << ", step " << step_;
    placeVote(std::move(vote));
  }
}

void PbftManager::certifyBlock_() {
  // The Certifying Step
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_dg_) << "PBFT certifying state in round: " << round << ", period: " << period;

  go_finish_state_ = elapsed_time_in_round_ms_ > 4 * LAMBDA_ms - POLLING_INTERVAL_ms;

  if (elapsed_time_in_round_ms_ < 2 * LAMBDA_ms) {
    // Should not happen, add log here for safety checking
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only " << elapsed_time_in_round_ms_ << " (ms) in round "
                 << round;
    return;
  }

  if (go_finish_state_) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in round " << round;
    return;
  }

  // Already sent (or should have) cert voted in this round
  if (should_have_cert_voted_in_this_round_) {
    LOG(log_dg_) << "Already did (or should have) cert vote in this round " << round;
    return;
  }

  // Get soft voted bock with 2t+1 soft votes
  const auto soft_voted_block = getSoftVotedBlockForThisRound_();
  if (soft_voted_block.has_value() == false) {
    LOG(log_dg_) << "Certify: Not enough soft votes for current round yet. Round " << round << ", period " << period;
    return;
  }
  const auto [current_round_soft_voted_block, current_round_soft_votes_period] = *soft_voted_block;

  if (!compareBlocksAndRewardVotes_(current_round_soft_voted_block)) {
    LOG(log_dg_) << "Incomplete or invalid soft voted block " << current_round_soft_voted_block << ", round " << round
                 << ", period " << period;
    return;
  }

  LOG(log_tr_) << "Finished compareBlocksAndRewardVotes_";

  // TODO CONCERN: If we do happen to see 2t+1 cert votes for a value before we have manged
  //               to cert vote it on our own, then we should issue a reward vote to our self no?
  /*
  // NOTE: If we have already executed this round then block won't be found in unverified queue...
  bool executed_soft_voted_block_for_this_round = false;

  if (have_executed_this_round_) {
    LOG(log_tr_) << "Have already executed before certifying in step 3 in round " << round;
    auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
    if (last_pbft_block_hash == current_round_soft_voted_block) {
      LOG(log_tr_) << "Having executed, last block in chain is the soft voted block in round " << round;
      executed_soft_voted_block_for_this_round = true;
    }
  }*/

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash == current_round_soft_voted_block) {
    LOG(log_er_) << "In certify step, soft voted block is already in chain!" << round;
    assert(false);
  }

  bool unverified_soft_vote_block_for_this_round_is_valid = false;
  auto block = pbft_chain_->getUnverifiedPbftBlock(current_round_soft_voted_block);
  if (block && pbft_chain_->checkPbftBlockValidation(*block)) {
    unverified_soft_vote_block_for_this_round_is_valid = true;
  } else {
    if (!block) {
      LOG(log_er_) << "Cannot find the unverified pbft block " << current_round_soft_voted_block << " in round "
                   << round << ", step 3";
    }
    syncPbftChainFromPeers_(invalid_soft_voted_block, current_round_soft_voted_block);
  }

  // CONCERN: We really should avoid doing these checks in both soft vote and cert vote steps!!

  if (unverified_soft_vote_block_for_this_round_is_valid) {
    // compareBlocksAndRewardVotes_ has checked the cert voted block exist
    auto cert_voted_block = getUnfinalizedBlock_(current_round_soft_voted_block);
    assert(cert_voted_block != nullptr);

    if (cert_voted_block->getPeriod() != current_round_soft_votes_period) {
      LOG(log_er_) << "CertifyBlock: Soft voted block period " << cert_voted_block->getPeriod()
                   << " != 2t+1 soft votes period " << current_round_soft_votes_period << ", round " << round;
      return;
    }

    cert_voted_block_for_round_ = {cert_voted_block->getBlockHash(), cert_voted_block->getPeriod()};

    auto batch = db_->createWriteBatch();
    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::CertVotedBlockInRound, *cert_voted_block_for_round_, batch);
    db_->addPbftCertVotedBlockToBatch(*cert_voted_block, batch);
    db_->commitWriteBatch(batch);

    should_have_cert_voted_in_this_round_ = true;

    // generate cert vote
    if (auto vote = generateVoteWithWeight(cert_voted_block->getBlockHash(), cert_vote_type,
                                           cert_voted_block->getPeriod(), round, step_);
        vote) {
      LOG(log_nf_) << "Placed cert vote " << cert_voted_block->getBlockHash() << ", vote weight " << *vote->getWeight()
                   << ", round " << round << ", period " << cert_voted_block->getPeriod() << ", step " << step_;
      placeVote(std::move(vote));
    }
  }
}

void PbftManager::firstFinish_() {
  // Even number steps from 4 are in first finish
  auto [round, period] = getPbftRoundAndPeriod();
  const auto next_round_period = pbft_chain_->getPbftChainSize() + 1;
  LOG(log_dg_) << "PBFT first finishing state in round: " << round << ", period: " << period;

  if (cert_voted_block_for_round_.has_value()) {
    auto last_cert_voted_block = getUnfinalizedBlock_(cert_voted_block_for_round_->first);
    assert(last_cert_voted_block != nullptr);

    if (auto vote = generateVoteWithWeight(last_cert_voted_block->getBlockHash(), next_vote_type, next_round_period,
                                           round, step_);
        vote) {
      LOG(log_nf_) << "Placed first finish next vote for " << last_cert_voted_block->getBlockHash() << ", vote weight "
                   << *vote->getWeight() << ", round " << round << ", period " << last_cert_voted_block->getPeriod()
                   << ", step " << step_;
      placeVote(std::move(vote));
    }

    // Re-broadcast pbft block in case some nodes do not have it
    if (step_ % 20 == 0) {
      auto pbft_block = db_->getPbftCertVotedBlock(cert_voted_block_for_round_->first);
      assert(pbft_block);
      if (auto net = network_.lock()) {
        LOG(log_nf_) << "Rebroadcasting PBFT block: " << pbft_block->getBlockHash() << " and reward votes "
                     << pbft_block->getRewardVotes();
        net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
      }
    }
  } else if (round == 1 || (round >= 2 && (previous_round_next_voted_value_.first == NULL_BLOCK_HASH))) {
    // Starting value in round 1 is always null block hash... So combined with other condition for next
    // voting null block hash...
    if (auto vote = generateVoteWithWeight(NULL_BLOCK_HASH, next_vote_type, next_round_period, round, step_); vote) {
      LOG(log_nf_) << "Placed first finish next vote for " << NULL_BLOCK_HASH << ", vote weight " << *vote->getWeight()
                   << ", round " << round << ", period " << next_round_period << ", step " << step_;
      placeVote(std::move(vote));
    }
  } else {
    if (auto vote = generateVoteWithWeight(previous_round_next_voted_value_.first, next_vote_type, next_round_period,
                                           round, step_);
        vote) {
      LOG(log_nf_) << "Placed first finish next vote for " << previous_round_next_voted_value_.first.abridged()
                   << ", vote weight " << *vote->getWeight() << ", round " << round << ", period " << next_round_period
                   << ", step " << step_;
      placeVote(std::move(vote));
    }
  }
}

void PbftManager::secondFinish_() {
  // Odd number steps from 5 are in second finish
  auto [round, period] = getPbftRoundAndPeriod();
  const auto next_round_period = pbft_chain_->getPbftChainSize() + 1;
  LOG(log_dg_) << "PBFT second finishing state in round: " << round << ", period: " << period;

  assert(step_ >= startingStepInRound_);
  auto end_time_for_step = (2 + step_ - startingStepInRound_) * LAMBDA_ms - POLLING_INTERVAL_ms;
  LOG(log_tr_) << "Step " << step_ << " end time " << end_time_for_step;

  if (const auto soft_voted_block = getSoftVotedBlockForThisRound_(); soft_voted_block.has_value()) {
    const auto [current_round_soft_voted_block, current_round_soft_votes_period] = *soft_voted_block;

    if (current_round_soft_votes_period != period) {
      LOG(log_er_) << "Soft voted block " << current_round_soft_voted_block << " period "
                   << current_round_soft_votes_period << " != current pbft period " << period;
    }

    assert(current_round_soft_votes_period == period);

    auto soft_voted_block_votes = vote_mgr_->getVotesBundle(round, period, 2, two_t_plus_one_);
    if (soft_voted_block_votes.has_value()) {
      // Have enough soft votes for a voting value
      auto net = network_.lock();
      assert(net);  // Should never happen
      net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes(
          std::move(soft_voted_block_votes->votes));
      vote_mgr_->sendRewardVotes(getLastPbftBlockHash());
      LOG(log_dg_) << "Node has seen enough soft votes voted at " << soft_voted_block_votes->voted_block_hash
                   << ", regossip soft votes. In period " << period << ", round " << round << " step " << step_;
    }

    if (!next_voted_soft_value_) {
      if (auto vote = generateVoteWithWeight(current_round_soft_voted_block, next_vote_type, period, round, step_);
          vote) {
        LOG(log_nf_) << "Placed second finish vote for " << current_round_soft_voted_block << ", vote weight "
                     << *vote->getWeight() << ", period " << period << ", round " << round << ", step " << step_;
        placeVote(std::move(vote));
        db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
        next_voted_soft_value_ = true;
      }
    }
  } else if (!next_voted_null_block_hash_ && round >= 2 &&
             (previous_round_next_voted_value_.first == NULL_BLOCK_HASH) && !cert_voted_block_for_round_.has_value()) {
    if (auto vote = generateVoteWithWeight(NULL_BLOCK_HASH, next_vote_type, next_round_period, round, step_); vote) {
      LOG(log_nf_) << "Placed second finish next vote for " << NULL_BLOCK_HASH << ", vote weight " << *vote->getWeight()
                   << ", period " << next_round_period << ", round " << round << ", step " << step_;
      placeVote(std::move(vote));
      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      next_voted_null_block_hash_ = true;
    }
  }

  if (step_ > MAX_STEPS && (step_ - MAX_STEPS - 2) % 100 == 0) {
    syncPbftChainFromPeers_(exceeded_max_steps, NULL_BLOCK_HASH);
  }

  if (step_ > MAX_STEPS && (step_ - MAX_STEPS - 2) % 100 == 0 && !broadcastAlreadyThisStep_()) {
    LOG(log_dg_) << "Node " << node_addr_ << " broadcast next votes for previous round. In period " << period
                 << ", round " << round << " step " << step_;
    if (auto net = network_.lock()) {
      net->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->broadcastPreviousRoundNextVotesBundle();
    }
    pbft_round_last_broadcast_ = round;
    pbft_step_last_broadcast_ = step_;
  }

  loop_back_finish_state_ = elapsed_time_in_round_ms_ > end_time_for_step;
}

std::shared_ptr<PbftBlock> PbftManager::generatePbftBlock(const blk_hash_t &prev_blk_hash,
                                                          const blk_hash_t &anchor_hash, const blk_hash_t &order_hash) {
  const auto propose_period = pbft_chain_->getPbftChainSize() + 1;
  // Reward votes should only include those reward votes with the same round as the round last pbft block was pushed
  // into chain
  const auto reward_votes = vote_mgr_->getProposeRewardVotes();
  std::vector<vote_hash_t> reward_votes_hashes;
  std::transform(reward_votes.begin(), reward_votes.end(), std::back_inserter(reward_votes_hashes),
                 [](const auto &v) { return v->getHash(); });
  const auto pbft_block = std::make_shared<PbftBlock>(prev_blk_hash, anchor_hash, order_hash, propose_period,
                                                      node_addr_, node_sk_, std::move(reward_votes_hashes));

  // push pbft block
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);

  // broadcast pbft block
  if (auto net = network_.lock()) {
    net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
  }

  LOG(log_dg_) << node_addr_ << " propose PBFT block successful! in round: " << round_ << " in step: " << step_
               << " PBFT block: " << pbft_block->getBlockHash();

  return pbft_block;
}

std::shared_ptr<Vote> PbftManager::generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t period,
                                                uint64_t round, size_t step) {
  // sortition proof
  VrfPbftSortition vrf_sortition(vrf_sk_, {type, period, round, step});
  return std::make_shared<Vote>(node_sk_, std::move(vrf_sortition), blockhash);
}

std::pair<bool, std::string> PbftManager::validateVote(const std::shared_ptr<Vote> &vote) const {
  const uint64_t vote_period = vote->getPeriod();

  // Validate vote against dpos contract
  try {
    const auto pk = key_manager_->get(vote->getVoterAddr());
    if (!pk) {
      std::stringstream err;
      err << "No vrf key mapped for vote author " << vote->getVoterAddr();
      return {false, err.str()};
    }

    // TODO[1896]: final_chain_->dpos... ret values should be cached as it is heavily used and most of the time it
    // returns the same value!
    const uint64_t voter_dpos_votes_count =
        final_chain_->dpos_eligible_vote_count(vote_period - 1, vote->getVoterAddr());
    const uint64_t total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(vote_period - 1);
    const uint64_t pbft_sortition_threshold = getPbftSortitionThreshold(vote->getType(), vote_period - 1);

    vote->validate(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold, *pk);
  } catch (state_api::ErrFutureBlock &e) {
    std::stringstream err;
    err << "Unable to validate vote " << vote->getHash() << " against dpos contract. It's period (" << vote_period
        << ") is too far ahead of actual finalized pbft chain size (" << final_chain_->last_block_number()
        << "). Err msg: " << e.what();

    return {false, err.str()};
  } catch (const std::logic_error &e) {
    std::stringstream err;
    err << "Vote " << vote->getHash() << " validation failed. Err: " << e.what();

    return {false, err.str()};
  }

  return {true, ""};
}

uint64_t PbftManager::getCurrentPbftSortitionThreshold(PbftVoteTypes vote_type) const {
  switch (vote_type) {
    case propose_vote_type:
      return std::min<uint64_t>(NUMBER_OF_PROPOSERS, getDposTotalVotesCount());
    case soft_vote_type:
    case cert_vote_type:
    case next_vote_type:
    default:
      return sortition_threshold_;
  }
}

uint64_t PbftManager::getPbftSortitionThreshold(PbftVoteTypes vote_type, uint64_t pbft_period) const {
  // TODO[1896]: final_chain_->dpos... ret values should be cached as it is heavily used and most of the time it
  // returns the same value!
  const uint64_t total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(pbft_period);

  switch (vote_type) {
    case propose_vote_type:
      return std::min<uint64_t>(NUMBER_OF_PROPOSERS, total_dpos_votes_count);
    case soft_vote_type:
    case cert_vote_type:
    case next_vote_type:
    default:
      return std::min<uint64_t>(COMMITTEE_SIZE, total_dpos_votes_count);
  }
}

std::shared_ptr<Vote> PbftManager::generateVoteWithWeight(taraxa::blk_hash_t const &blockhash, PbftVoteTypes vote_type,
                                                          uint64_t period, uint64_t round, size_t step) {
  uint64_t voter_dpos_votes_count = 0;
  uint64_t total_dpos_votes_count = 0;
  uint64_t pbft_sortition_threshold = 0;

  try {
    // TODO[1896]: final_chain_->dpos... ret values should be cached as it is heavily used and most of the time it
    // returns the same value!
    voter_dpos_votes_count = final_chain_->dpos_eligible_vote_count(period - 1, node_addr_);
    total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(period - 1);
    pbft_sortition_threshold = getPbftSortitionThreshold(vote_type, period - 1);

  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_er_) << "Unable to place vote for round: " << round << ", period: " << period << ", step: " << step
                 << ", voted block hash: " << blockhash.abridged() << ". "
                 << "Period  is too far ahead of actual finalized pbft chain size ("
                 << final_chain_->last_block_number() << "). Err msg: " << e.what();

    return nullptr;
  }

  if (!voter_dpos_votes_count) {
    // No delegation
    return nullptr;
  }

  auto vote = generateVote(blockhash, vote_type, period, round, step);
  const auto weight = vote->calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold);

  if (weight) {
    if (auto is_unique_vote = vote_mgr_->isUniqueVote(vote); is_unique_vote.first) {
      db_->saveVerifiedVote(vote);
      vote_mgr_->addVerifiedVote(vote);
    } else {
      // This should never happen
      LOG(log_er_) << "Generated vote " << vote->getHash().abridged()
                   << " is not unique. Err: " << is_unique_vote.second;
      assert(false);
      return nullptr;
    }
  }

  return vote;
}

void PbftManager::placeVote(std::shared_ptr<Vote> &&vote) {
  if (auto net = network_.lock()) {
    net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes({std::move(vote)});
  }
}

blk_hash_t PbftManager::calculateOrderHash(const std::vector<blk_hash_t> &dag_block_hashes,
                                           const std::vector<trx_hash_t> &trx_hashes) {
  if (dag_block_hashes.empty()) {
    return NULL_BLOCK_HASH;
  }
  dev::RLPStream order_stream(2);
  order_stream.appendList(dag_block_hashes.size());
  for (auto const &blk_hash : dag_block_hashes) {
    order_stream << blk_hash;
  }
  order_stream.appendList(trx_hashes.size());
  for (auto const &trx_hash : trx_hashes) {
    order_stream << trx_hash;
  }
  return dev::sha3(order_stream.out());
}

blk_hash_t PbftManager::calculateOrderHash(const std::vector<DagBlock> &dag_blocks, const SharedTransactions &trxs) {
  if (dag_blocks.empty()) {
    return NULL_BLOCK_HASH;
  }
  dev::RLPStream order_stream(2);
  order_stream.appendList(dag_blocks.size());
  for (auto const &blk : dag_blocks) {
    order_stream << blk.getHash();
  }
  order_stream.appendList(trxs.size());
  for (auto const &trx : trxs) {
    order_stream << trx->getHash();
  }
  return dev::sha3(order_stream.out());
}

std::optional<blk_hash_t> findClosestAnchor(const std::vector<blk_hash_t> &ghost,
                                            const std::vector<blk_hash_t> &dag_order, uint32_t included) {
  for (uint32_t i = included; i > 0; i--) {
    if (std::count(ghost.begin(), ghost.end(), dag_order[i - 1])) {
      return dag_order[i - 1];
    }
  }
  return ghost[1];
}

std::shared_ptr<PbftBlock> PbftManager::proposePbftBlock_() {
  auto round = getPbftRound();
  const auto propose_period = pbft_chain_->getPbftChainSize() + 1;
  VrfPbftSortition vrf_sortition(vrf_sk_, {propose_vote_type, propose_period, round, 1});
  if (weighted_votes_count_ == 0 ||
      !vrf_sortition.calculateWeight(getDposWeightedVotesCount(), getDposTotalVotesCount(),
                                     getCurrentPbftSortitionThreshold(propose_vote_type), dev::toPublic(node_sk_))) {
    return nullptr;
  }

  LOG(log_dg_) << "Into propose PBFT block";
  blk_hash_t last_period_dag_anchor_block_hash;
  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    auto prev_block_hash = last_pbft_block_hash;
    auto prev_pbft_block = pbft_chain_->getPbftBlockInChain(prev_block_hash);

    if (prev_pbft_block.getPeriod() != propose_period - 1) [[unlikely]] {
      LOG(log_er_) << "Last pbft block " << prev_block_hash.abridged() << " period " << prev_pbft_block.getPeriod()
                   << " differs from chain size " << propose_period << ". Possible reason - race condition";
      return nullptr;
    }

    last_period_dag_anchor_block_hash = prev_pbft_block.getPivotDagBlockHash();
    while (!last_period_dag_anchor_block_hash) {
      // The anchor is NULL BLOCK HASH
      prev_block_hash = prev_pbft_block.getPrevBlockHash();
      if (!prev_block_hash) {
        // The genesis PBFT block head
        last_period_dag_anchor_block_hash = dag_genesis_block_hash_;
        break;
      }
      prev_pbft_block = pbft_chain_->getPbftBlockInChain(prev_block_hash);
      last_period_dag_anchor_block_hash = prev_pbft_block.getPivotDagBlockHash();
    }
  } else {
    // First PBFT block
    last_period_dag_anchor_block_hash = dag_genesis_block_hash_;
  }

  std::vector<blk_hash_t> ghost;
  dag_mgr_->getGhostPath(last_period_dag_anchor_block_hash, ghost);
  LOG(log_dg_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at least include the last period dag anchor block
  if (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(last_pbft_block_hash, NULL_BLOCK_HASH, NULL_BLOCK_HASH);
  }

  blk_hash_t dag_block_hash;
  if (ghost.size() <= DAG_BLOCKS_SIZE) {
    // Move back GHOST_PATH_MOVE_BACK DAG blocks for DAG sycning
    auto ghost_index = (ghost.size() < GHOST_PATH_MOVE_BACK + 1) ? 0 : (ghost.size() - 1 - GHOST_PATH_MOVE_BACK);
    while (ghost_index < ghost.size() - 1) {
      if (ghost[ghost_index] != last_period_dag_anchor_block_hash) {
        break;
      }
      ghost_index += 1;
    }
    dag_block_hash = ghost[ghost_index];
  } else {
    dag_block_hash = ghost[DAG_BLOCKS_SIZE - 1];
  }

  if (dag_block_hash == dag_genesis_block_hash_) {
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis " << dag_block_hash
                 << " PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(last_pbft_block_hash, NULL_BLOCK_HASH, NULL_BLOCK_HASH);
  }

  // Compare with last dag block hash. If they are same, which means no new dag blocks generated since last round. In
  // that case PBFT proposer should propose NULL BLOCK HASH anchor as their value
  if (dag_block_hash == last_period_dag_anchor_block_hash) {
    LOG(log_dg_) << "Last period DAG anchor block hash " << dag_block_hash
                 << " No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    LOG(log_dg_) << "Ghost: " << ghost;
    return generatePbftBlock(last_pbft_block_hash, NULL_BLOCK_HASH, NULL_BLOCK_HASH);
  }

  // get DAG block and transaction order
  auto dag_block_order = dag_mgr_->getDagBlockOrder(dag_block_hash, propose_period);

  if (dag_block_order.empty()) {
    LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlockOrder failed in propose";
    assert(false);
  }

  std::unordered_set<trx_hash_t> trx_hashes_set;
  std::vector<trx_hash_t> trx_hashes;
  u256 total_weight = 0;
  uint32_t dag_blocks_included = 0;
  for (auto const &blk_hash : dag_block_order) {
    auto dag_blk = dag_mgr_->getDagBlock(blk_hash);
    if (!dag_blk) {
      LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlock failed in propose for block "
                   << blk_hash;
      assert(false);
    }
    u256 dag_block_weight = 0;
    const auto &estimations = dag_blk->getTrxsGasEstimations();

    int32_t i = 0;
    for (const auto &trx_hash : dag_blk->getTrxs()) {
      if (trx_hashes_set.emplace(trx_hash).second) {
        trx_hashes.emplace_back(trx_hash);
        dag_block_weight += estimations[i];
      }
      i++;
    }
    if (total_weight + dag_block_weight > config_.gas_limit) {
      // we need to form new list of transactions after clipping if block is overweighted
      trx_hashes.clear();
      break;
    }
    total_weight += dag_block_weight;
    dag_blocks_included++;
  }

  if (dag_blocks_included != dag_block_order.size()) {
    auto closest_anchor = findClosestAnchor(ghost, dag_block_order, dag_blocks_included);
    if (!closest_anchor) {
      LOG(log_er_) << "Can't find closest anchor after block clipping. Ghost: " << ghost << ". Clipped block_order: "
                   << vec_blk_t(dag_block_order.begin(), dag_block_order.begin() + dag_blocks_included);
      assert(false);
    }

    dag_block_hash = *closest_anchor;
    dag_block_order = dag_mgr_->getDagBlockOrder(dag_block_hash, propose_period);
  }
  if (trx_hashes.empty()) {
    std::unordered_set<trx_hash_t> trx_set;
    std::vector<trx_hash_t> transactions_to_query;
    for (auto const &dag_blk_hash : dag_block_order) {
      auto dag_block = dag_mgr_->getDagBlock(dag_blk_hash);
      assert(dag_block);
      for (auto const &trx_hash : dag_block->getTrxs()) {
        if (trx_set.insert(trx_hash).second) {
          trx_hashes.emplace_back(trx_hash);
        }
      }
    }
  }
  std::vector<trx_hash_t> non_finalized_transactions;
  auto trx_finalized = db_->transactionsFinalized(trx_hashes);
  for (uint32_t i = 0; i < trx_finalized.size(); i++) {
    if (!trx_finalized[i]) {
      non_finalized_transactions.emplace_back(trx_hashes[i]);
    }
  }

  const auto transactions = trx_mgr_->getNonfinalizedTrx(non_finalized_transactions, true /*sorted*/);
  non_finalized_transactions.clear();
  std::transform(transactions.begin(), transactions.end(), std::back_inserter(non_finalized_transactions),
                 [](const auto &t) { return t->getHash(); });

  auto order_hash = calculateOrderHash(dag_block_order, non_finalized_transactions);
  auto pbft_block_hash = generatePbftBlock(last_pbft_block_hash, dag_block_hash, order_hash);
  LOG(log_nf_) << "Proposed Pbft block: " << pbft_block_hash << ". Order hash:" << order_hash
               << ". DAG order for proposed block" << dag_block_order << ". Transaction order for proposed block"
               << non_finalized_transactions;

  return pbft_block_hash;
}

h256 PbftManager::getProposal(const std::shared_ptr<Vote> &vote) const {
  auto lowest_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), 1);
  for (uint64_t i = 2; i <= vote->getWeight(); ++i) {
    auto tmp_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), i);
    if (lowest_hash > tmp_hash) {
      lowest_hash = tmp_hash;
    }
  }
  return lowest_hash;
}

std::optional<std::pair<blk_hash_t, uint64_t>> PbftManager::identifyLeaderBlock_(uint64_t round, uint64_t period) {
  LOG(log_tr_) << "Identify leader block, in period " << period << ", round " << round;

  // Get all proposal votes in the round
  auto votes = vote_mgr_->getProposalVotes(round, period);

  // Each leader candidate with <vote_signature_hash, vote>
  std::vector<std::pair<h256, std::shared_ptr<Vote>>> leader_candidates;

  for (auto const &v : votes) {
    if (v->getRound() != round) {
      LOG(log_er_) << "Vote round is different than current round " << round << ". Vote " << v;
      continue;
    }

    if (v->getPeriod() != period) {
      LOG(log_er_) << "Vote period is different than wanted new period " << period << ". Vote " << v;
      continue;
    }

    if (v->getStep() != 1) {
      LOG(log_er_) << "Vote step is not 1. Vote " << v;
      continue;
    }

    if (v->getType() != propose_vote_type) {
      LOG(log_er_) << "Vote type is not propose vote type. Vote " << v;
      continue;
    }

    const auto proposed_block_hash = v->getBlockHash();
    if (proposed_block_hash == NULL_BLOCK_HASH) {
      LOG(log_er_) << "Propose block hash should not be NULL. Vote " << v;
      continue;
    }

    if (pbft_chain_->findPbftBlockInChain(proposed_block_hash)) {
      continue;
    }

    auto pbft_block = pbft_chain_->getUnverifiedPbftBlock(proposed_block_hash);
    if (!pbft_block) {
      LOG(log_er_) << "Unable to find proposed block " << proposed_block_hash;
      continue;
    }

    if (!compareBlocksAndRewardVotes_(pbft_block->getBlockHash())) {
      LOG(log_er_) << "Incomplete or invalid proposed block " << pbft_block->getBlockHash() << ", period " << period
                   << ", round " << round;
      continue;
    }

    if (!pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      LOG(log_er_) << "Proposed block " << pbft_block->getBlockHash() << " failed validation, period " << period
                   << ", round " << round;
      continue;
    }

    leader_candidates.emplace_back(std::make_pair(getProposal(v), v));
  }

  if (leader_candidates.empty()) {
    // no eligible leader
    return {};
  }

  const auto leader = *std::min_element(leader_candidates.begin(), leader_candidates.end(),
                                        [](const auto &i, const auto &j) { return i.first < j.first; });

  return {std::make_pair(leader.second->getBlockHash(), leader.second->getPeriod())};
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return getPbftRound() == pbft_round_last_requested_sync_ && step_ == pbft_step_last_requested_sync_;
}

void PbftManager::syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash) {
  if (stopped_) {
    return;
  }
  if (auto net = network_.lock()) {
    if (periodDataQueueSize()) {
      LOG(log_tr_) << "PBFT synced queue is still processing so skip syncing. Synced queue size "
                   << periodDataQueueSize();

      return;
    }

    if (!is_syncing_() && !syncRequestedAlreadyThisStep_()) {
      auto round = getPbftRound();

      switch (reason) {
        case missing_dag_blk:
          LOG(log_nf_) << "DAG blocks have not synced yet, anchor block " << relevant_blk_hash
                       << " not present in DAG.";
          break;
        case invalid_cert_voted_block:
          // Get partition, need send request to get missing pbft blocks from peers
          LOG(log_nf_) << "Cert voted block " << relevant_blk_hash
                       << " is invalid, we must be out of sync with pbft chain.";
          break;
        case invalid_soft_voted_block:
          // TODO: Address CONCERN of should we sync here?  Any malicious player can propose an invalid soft voted
          // block... Honest nodes will soft vote for any malicious block before receiving it and verifying it.
          LOG(log_nf_) << "Soft voted block for this round appears to be invalid, perhaps node out of sync";
          break;
        case exceeded_max_steps:
          LOG(log_nf_) << "Suspect consensus is partitioned, reached step " << step_ << " in round " << round
                       << " without advancing.";
          // We want to force sycning the DAG...
          break;
        default:
          LOG(log_er_) << "Unknown PBFT sync request reason " << reason;
          assert(false);

          LOG(log_nf_) << "Restarting sync in round " << round << ", step " << step_;
      }

      // TODO: Is there any need to sync here???
      // If we detect some stalled situation, a better step would be to disconnect/reconnect to nodes to try to get
      // network unstuck since reconnecting both shuffles the nodes and invokes pbft/dag syncing if needed
      // net->restartSyncingPbft();

      pbft_round_last_requested_sync_ = round;
      pbft_step_last_requested_sync_ = step_;
    }
  }
}

bool PbftManager::broadcastAlreadyThisStep_() const {
  return getPbftRound() == pbft_round_last_broadcast_ && step_ == pbft_step_last_broadcast_;
}

bool PbftManager::compareBlocksAndRewardVotes_(const blk_hash_t &pbft_block_hash) {
  auto pbft_block = getUnfinalizedBlock_(pbft_block_hash);
  if (!pbft_block) {
    return false;
  }

  return compareBlocksAndRewardVotes_(std::move(pbft_block)).second;
}

std::pair<vec_blk_t, bool> PbftManager::compareBlocksAndRewardVotes_(std::shared_ptr<PbftBlock> pbft_block) {
  vec_blk_t dag_blocks_order;
  const auto proposal_block_hash = pbft_block->getBlockHash();
  // If cert period data is already populated with this pbft block, no need to do verification again
  if (period_data_.pbft_blk && period_data_.pbft_blk->getBlockHash() == proposal_block_hash) {
    dag_blocks_order.reserve(period_data_.dag_blocks.size());
    std::transform(period_data_.dag_blocks.begin(), period_data_.dag_blocks.end(), std::back_inserter(dag_blocks_order),
                   [](const DagBlock &dag_block) { return dag_block.getHash(); });
    return std::make_pair(std::move(dag_blocks_order), true);
  }

  // Check reward votes
  if (!vote_mgr_->checkRewardVotes(pbft_block)) {
    LOG(log_er_) << "Failed verifying reward votes for proposed PBFT block " << proposal_block_hash;
    return {{}, false};
  }

  auto const &anchor_hash = pbft_block->getPivotDagBlockHash();
  if (anchor_hash == NULL_BLOCK_HASH) {
    period_data_.clear();
    period_data_.pbft_blk = std::move(pbft_block);
    return std::make_pair(std::move(dag_blocks_order), true);
  }

  dag_blocks_order = dag_mgr_->getDagBlockOrder(anchor_hash, pbft_block->getPeriod());
  if (dag_blocks_order.empty()) {
    LOG(log_er_) << "Missing dag blocks for proposed PBFT block " << proposal_block_hash;
    syncPbftChainFromPeers_(missing_dag_blk, anchor_hash);
    return std::make_pair(std::move(dag_blocks_order), false);
  }

  std::unordered_set<trx_hash_t> trx_set;
  std::vector<trx_hash_t> transactions_to_query;
  period_data_.clear();
  period_data_.dag_blocks.reserve(dag_blocks_order.size());
  for (auto const &dag_blk_hash : dag_blocks_order) {
    auto dag_block = dag_mgr_->getDagBlock(dag_blk_hash);
    assert(dag_block);
    for (auto const &trx_hash : dag_block->getTrxs()) {
      if (trx_set.insert(trx_hash).second) {
        transactions_to_query.emplace_back(trx_hash);
      }
    }
    period_data_.dag_blocks.emplace_back(std::move(*dag_block));
  }
  std::vector<trx_hash_t> non_finalized_transactions;
  auto trx_finalized = db_->transactionsFinalized(transactions_to_query);
  for (uint32_t i = 0; i < trx_finalized.size(); i++) {
    if (!trx_finalized[i]) {
      non_finalized_transactions.emplace_back(transactions_to_query[i]);
    }
  }

  const auto transactions = trx_mgr_->getNonfinalizedTrx(non_finalized_transactions, true /*sorted*/);
  if (transactions.size() < non_finalized_transactions.size()) {
    LOG(log_er_) << "Missing transactions for proposed PBFT block " << proposal_block_hash;
    return std::make_pair(std::move(dag_blocks_order), false);
  }

  non_finalized_transactions.clear();

  period_data_.transactions.reserve(transactions.size());
  for (const auto &trx : transactions) {
    non_finalized_transactions.push_back(trx->getHash());
    period_data_.transactions.push_back(trx);
  }

  auto calculated_order_hash = calculateOrderHash(dag_blocks_order, non_finalized_transactions);
  if (calculated_order_hash != pbft_block->getOrderHash()) {
    LOG(log_er_) << "Order hash incorrect. Pbft block: " << proposal_block_hash
                 << ". Order hash: " << pbft_block->getOrderHash() << " . Calculated hash:" << calculated_order_hash
                 << ". Dag order: " << dag_blocks_order << ". Trx order: " << non_finalized_transactions;
    dag_blocks_order.clear();
    return std::make_pair(std::move(dag_blocks_order), false);
  }

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    auto prev_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);

    std::vector<blk_hash_t> ghost;
    dag_mgr_->getGhostPath(prev_pbft_block.getPivotDagBlockHash(), ghost);
    if (ghost.size() > 1 && anchor_hash != ghost[1]) {
      if (!checkBlockWeight(period_data_)) {
        LOG(log_er_) << "PBFT block " << proposal_block_hash << " is overweighted";
        return std::make_pair(std::move(dag_blocks_order), false);
      }
    }
  }

  period_data_.pbft_blk = std::move(pbft_block);

  return std::make_pair(std::move(dag_blocks_order), true);
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(taraxa::blk_hash_t const &cert_voted_block_hash,
                                                   std::vector<std::shared_ptr<Vote>> &&current_round_cert_votes) {
  auto pbft_block = getUnfinalizedBlock_(cert_voted_block_hash);
  if (!pbft_block) {
    LOG(log_er_) << "Can not find the cert voted block hash " << cert_voted_block_hash << " in both pbft queue and DB";
    return false;
  }

  if (!pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
    LOG(log_er_) << "Failed pbft chain validation for cert voted block " << cert_voted_block_hash
                 << ", will call sync pbft chain from peers";
    syncPbftChainFromPeers_(invalid_cert_voted_block, cert_voted_block_hash);
    return false;
  }

  auto [dag_blocks_order, ok] = compareBlocksAndRewardVotes_(pbft_block);
  if (!ok) {
    LOG(log_er_) << "Failed compare DAG blocks or reward votes with cert voted block " << cert_voted_block_hash;
    return false;
  }

  period_data_.previous_block_cert_votes = vote_mgr_->getRewardVotesByHashes(period_data_.pbft_blk->getRewardVotes());
  if (period_data_.previous_block_cert_votes.size() < period_data_.pbft_blk->getRewardVotes().size()) {
    LOG(log_er_) << "Missing reward votes in cert voted block " << cert_voted_block_hash;
    return false;
  }

  if (!pushPbftBlock_(std::move(period_data_), std::move(current_round_cert_votes), std::move(dag_blocks_order))) {
    LOG(log_er_) << "Failed push cert voted block " << pbft_block->getBlockHash() << " into PBFT chain";
    return false;
  }

  // cleanup PBFT unverified blocks table
  pbft_chain_->cleanupUnverifiedPbftBlocks(*pbft_block);

  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain() {
  if (auto net = network_.lock()) {
    auto round = getPbftRound();
    while (periodDataQueueSize() > 0) {
      auto period_data_opt = processPeriodData();
      if (!period_data_opt) continue;
      auto period_data = std::move(*period_data_opt);
      const auto period = period_data.first.pbft_blk->getPeriod();
      auto pbft_block_hash = period_data.first.pbft_blk->getBlockHash();
      LOG(log_nf_) << "Pick pbft block " << pbft_block_hash << " from synced queue in round " << round;

      if (pushPbftBlock_(std::move(period_data.first), std::move(period_data.second))) {
        LOG(log_nf_) << node_addr_ << " push synced PBFT block " << pbft_block_hash << " in period " << period
                     << ", round " << round;
      } else {
        LOG(log_si_) << "Failed push PBFT block " << pbft_block_hash << " into chain";
        break;
      }

      net->setSyncStatePeriod(period);

      if (executed_pbft_block_) {
        updateDposState_();
        // update sortition_threshold and TWO_T_PLUS_ONE
        updateTwoTPlusOneAndThreshold_();
        db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
        executed_pbft_block_ = false;
      }
    }
  }
}

void PbftManager::finalize_(PeriodData &&period_data, std::vector<h256> &&finalized_dag_blk_hashes, bool sync) {
  const auto anchor = period_data.pbft_blk->getPivotDagBlockHash();

  auto result = final_chain_->finalize(
      std::move(period_data), std::move(finalized_dag_blk_hashes),
      [this, weak_ptr = weak_from_this(), anchor_hash = anchor, period = period_data.pbft_blk->getPeriod()](
          auto const &, auto &batch) {
        // Update proposal period DAG levels map
        auto ptr = weak_ptr.lock();
        if (!ptr) return;  // it was destroyed

        if (!anchor_hash) {
          // Null anchor don't update proposal period DAG levels map
          return;
        }

        auto anchor = dag_mgr_->getDagBlock(anchor_hash);
        if (!anchor) {
          LOG(log_er_) << "DB corrupted - Cannot find anchor block: " << anchor_hash << " in DB.";
          assert(false);
        }

        db_->addProposalPeriodDagLevelsMapToBatch(anchor->getLevel() + max_levels_per_period_, period, batch);
      });

  if (sync) {
    result.wait();
  }
}

bool PbftManager::pushPbftBlock_(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&cert_votes,
                                 vec_blk_t &&dag_blocks_order) {
  auto const &pbft_block_hash = period_data.pbft_blk->getBlockHash();
  if (db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_nf_) << "PBFT block: " << pbft_block_hash << " in DB already.";
    if (cert_voted_block_for_round_.has_value() && cert_voted_block_for_round_->first == pbft_block_hash) {
      LOG(log_er_) << "Last cert voted value should be NULL_BLOCK_HASH. Block hash "
                   << cert_voted_block_for_round_->first << " has been pushed into chain already";
      assert(false);
    }
    return false;
  }

  assert(cert_votes.empty() == false);
  assert(pbft_block_hash == cert_votes[0]->getBlockHash());

  auto pbft_period = period_data.pbft_blk->getPeriod();
  auto null_anchor = period_data.pbft_blk->getPivotDagBlockHash() == NULL_BLOCK_HASH;

  auto batch = db_->createWriteBatch();

  LOG(log_nf_) << "Storing cert votes of pbft blk " << pbft_block_hash;
  LOG(log_dg_) << "Stored following cert votes:\n" << cert_votes;
  // Update PBFT chain head block
  db_->addPbftHeadToBatch(pbft_chain_->getHeadHash(), pbft_chain_->getJsonStrForBlock(pbft_block_hash, null_anchor),
                          batch);

  if (dag_blocks_order.empty()) {
    dag_blocks_order.reserve(period_data.dag_blocks.size());
    std::transform(period_data.dag_blocks.begin(), period_data.dag_blocks.end(), std::back_inserter(dag_blocks_order),
                   [](const DagBlock &dag_block) { return dag_block.getHash(); });
  }

  db_->savePeriodData(period_data, batch);
  db_->addLastBlockCertVotesToBatch(cert_votes, vote_mgr_->getAllRewardVotes(), batch);

  // pass pbft with dag blocks and transactions to adjust difficulty
  if (period_data.pbft_blk->getPivotDagBlockHash() != NULL_BLOCK_HASH) {
    dag_mgr_->sortitionParamsManager().pbftBlockPushed(period_data, batch,
                                                       pbft_chain_->getPbftChainSizeExcludingEmptyPbftBlocks() + 1);
  }
  {
    // This makes sure that no DAG block or transaction can be added or change state in transaction and dag manager when
    // finalizing pbft block with dag blocks and transactions
    std::unique_lock dag_lock(dag_mgr_->getDagMutex());
    std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());

    // Commit DB
    db_->commitWriteBatch(batch);

    // Set DAG blocks period
    auto const &anchor_hash = period_data.pbft_blk->getPivotDagBlockHash();
    dag_mgr_->setDagBlockOrder(anchor_hash, pbft_period, dag_blocks_order);

    trx_mgr_->updateFinalizedTransactionsStatus(period_data);

    // update PBFT chain size
    pbft_chain_->updatePbftChain(pbft_block_hash, null_anchor);
  }

  // Advance pbft consensus period
  advancePeriod();

  vote_mgr_->replaceRewardVotes(std::move(cert_votes));

  LOG(log_nf_) << "Pushed new PBFT block " << pbft_block_hash << " into chain. Period: " << pbft_period
               << ", round: " << getPbftRound();

  finalize_(std::move(period_data), std::move(dag_blocks_order));

  db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  executed_pbft_block_ = true;
  return true;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  // Update 2t+1 and threshold
  auto dpos_total_votes_count = getDposTotalVotesCount();
  sortition_threshold_ = std::min<size_t>(COMMITTEE_SIZE, dpos_total_votes_count);
  two_t_plus_one_ = sortition_threshold_ * 2 / 3 + 1;
  LOG(log_nf_) << "Committee size " << COMMITTEE_SIZE << ", DPOS total votes count " << dpos_total_votes_count
               << ". Update 2t+1 " << two_t_plus_one_ << ", Threshold " << sortition_threshold_;
}

std::shared_ptr<PbftBlock> PbftManager::getUnfinalizedBlock_(blk_hash_t const &block_hash) {
  auto block = pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!block) {
    block = db_->getPbftCertVotedBlock(block_hash);
    if (block) {
      // PBFT unverified queue empty after node reboot, read from DB pushing back in unverified queue
      pbft_chain_->pushUnverifiedPbftBlock(block);
    }
  }

  return block;
}

void PbftManager::countVotes_() const {
  auto round = getPbftRound();
  while (!monitor_stop_) {
    auto verified_votes = vote_mgr_->getVerifiedVotes();
    std::vector<std::shared_ptr<Vote>> votes;
    votes.reserve(verified_votes.size());
    votes.insert(votes.end(), std::make_move_iterator(verified_votes.begin()),
                 std::make_move_iterator(verified_votes.end()));

    size_t last_step_votes = 0;
    size_t current_step_votes = 0;
    for (auto const &v : votes) {
      if (step_ == 1) {
        if (v->getRound() == round - 1 && v->getStep() == last_step_) {
          last_step_votes++;
        } else if (v->getRound() == round && v->getStep() == step_) {
          current_step_votes++;
        }
      } else {
        if (v->getRound() == round) {
          if (v->getStep() == step_ - 1) {
            last_step_votes++;
          } else if (v->getStep() == step_) {
            current_step_votes++;
          }
        }
      }
    }

    auto now = std::chrono::system_clock::now();
    auto last_step_duration = now - last_step_clock_initial_datetime_;
    auto elapsed_last_step_time_in_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(last_step_duration).count();

    auto current_step_duration = now - current_step_clock_initial_datetime_;
    auto elapsed_current_step_time_in_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(current_step_duration).count();

    LOG(log_nf_test_) << "Round " << round << " step " << last_step_ << " time " << elapsed_last_step_time_in_ms
                      << "(ms) has " << last_step_votes << " votes";
    LOG(log_nf_test_) << "Round " << round << " step " << step_ << " time " << elapsed_current_step_time_in_ms
                      << "(ms) has " << current_step_votes << " votes";
    thisThreadSleepForMilliSeconds(POLLING_INTERVAL_ms / 2);
  }
}

bool PbftManager::is_syncing_() {
  if (auto net = network_.lock()) {
    return net->pbft_syncing();
  }
  return false;
}

uint64_t PbftManager::pbftSyncingPeriod() const {
  return std::max(sync_queue_.getPeriod(), pbft_chain_->getPbftChainSize());
}

std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>> PbftManager::processPeriodData() {
  auto [period_data, cert_votes, node_id] = sync_queue_.pop();
  auto pbft_block_hash = period_data.pbft_blk->getBlockHash();
  LOG(log_nf_) << "Pop pbft block " << pbft_block_hash << " from synced queue";

  if (pbft_chain_->findPbftBlockInChain(pbft_block_hash)) {
    LOG(log_dg_) << "PBFT block " << pbft_block_hash << " already present in chain.";
    return std::nullopt;
  }

  auto net = network_.lock();
  assert(net);  // Should never happen

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();

  // Check previous hash matches
  if (period_data.pbft_blk->getPrevBlockHash() != last_pbft_block_hash) {
    auto last_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    if (period_data.pbft_blk->getPeriod() <= last_pbft_block.getPeriod()) {
      // Old block in the sync queue
      return std::nullopt;
    }

    LOG(log_er_) << "Invalid PBFT block " << pbft_block_hash
                 << "; prevHash: " << period_data.pbft_blk->getPrevBlockHash() << " from peer " << node_id.abridged()
                 << " received, stop syncing.";
    sync_queue_.clear();
    // Handle malicious peer on network level
    net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  // Check reward votes
  if (!vote_mgr_->checkRewardVotes(period_data.pbft_blk)) {
    LOG(log_er_) << "Failed verifying reward votes. Disconnect malicious peer " << node_id.abridged();
    sync_queue_.clear();
    net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  // Special case when previous block was already in chain so we hit condition
  // pbft_chain_->findPbftBlockInChain(pbft_block_hash) and it's cert votes were not verified here, they are part of
  // vote_manager so we need to replace them as they are not verified period_data structure
  if (period_data.previous_block_cert_votes.size() && !period_data.previous_block_cert_votes.front()->getWeight()) {
    if (auto votes = vote_mgr_->getRewardVotesByHashes(period_data.pbft_blk->getRewardVotes()); votes.size()) {
      if (votes.size() < period_data.pbft_blk->getRewardVotes().size()) {
        LOG(log_er_) << "Failed verifying reward votes. PBFT block " << pbft_block_hash << ".Disconnect malicious peer "
                     << node_id.abridged();
        sync_queue_.clear();
        net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
        return std::nullopt;
      }
      period_data.previous_block_cert_votes = std::move(votes);
    }
  }

  // Check cert votes validation
  try {
    period_data.hasEnoughValidCertVotes(
        cert_votes, getDposTotalVotesCount(), getCurrentPbftSortitionThreshold(cert_vote_type), getTwoTPlusOne(),
        [this](auto const &addr) { return dposEligibleVoteCount_(addr); },
        [this](auto const &addr) { return key_manager_->get(addr); });
  } catch (const std::logic_error &e) {
    // Failed cert votes validation, flush synced PBFT queue and set since
    // next block validation depends on the current one
    LOG(log_er_) << e.what();  // log exception text from hasEnoughValidCertVotes
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash
                 << " doesn't have enough valid cert votes. Clear synced PBFT blocks! DPOS total votes count: "
                 << getDposTotalVotesCount();
    sync_queue_.clear();
    net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  return std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>>(
      {std::move(period_data), std::move(cert_votes)});
}

blk_hash_t PbftManager::lastPbftBlockHashFromQueueOrChain() {
  auto pbft_block = sync_queue_.lastPbftBlock();
  if (pbft_block) return pbft_block->getBlockHash();
  return pbft_chain_->getLastPbftBlockHash();
}

bool PbftManager::periodDataQueueEmpty() const { return sync_queue_.empty(); }

void PbftManager::periodDataQueuePush(PeriodData &&period_data, dev::p2p::NodeID const &node_id,
                                      std::vector<std::shared_ptr<Vote>> &&current_block_cert_votes) {
  const auto period = period_data.pbft_blk->getPeriod();
  if (!sync_queue_.push(std::move(period_data), node_id, pbft_chain_->getPbftChainSize(),
                        std::move(current_block_cert_votes))) {
    LOG(log_er_) << "Trying to push period data with " << period << " period, but current period is "
                 << sync_queue_.getPeriod();
  }
}

size_t PbftManager::periodDataQueueSize() const { return sync_queue_.size(); }

std::unordered_map<trx_hash_t, u256> getAllTrxEstimations(const PeriodData &period_data) {
  std::unordered_map<trx_hash_t, u256> result;
  for (const auto &dag_block : period_data.dag_blocks) {
    const auto &transactions = dag_block.getTrxs();
    const auto &estimations = dag_block.getTrxsGasEstimations();
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      result.emplace(transactions[i], estimations[i]);
    }
  }
  return result;
}

bool PbftManager::checkBlockWeight(const PeriodData &period_data) const {
  const auto &trx_estimations = getAllTrxEstimations(period_data);

  u256 total_weight = 0;
  for (const auto &tx : period_data.transactions) {
    total_weight += trx_estimations.at(tx->getHash());
  }
  if (total_weight > config_.gas_limit) {
    return false;
  }
  return true;
}

blk_hash_t PbftManager::getLastPbftBlockHash() { return pbft_chain_->getLastPbftBlockHash(); }

}  // namespace taraxa
