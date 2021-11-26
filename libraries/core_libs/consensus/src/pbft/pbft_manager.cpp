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
#include <string>

#include "dag/dag.hpp"
#include "final_chain/final_chain.hpp"
#include "votes/rewards_votes.hpp"
#include "votes/vote_manager.hpp"

namespace taraxa {
using vrf_output_t = vrf_wrapper::vrf_output_t;

PbftManager::PbftManager(PbftConfig const &conf, blk_hash_t const &genesis, addr_t node_addr,
                         std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
                         std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                         std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<final_chain::FinalChain> final_chain,
                         std::shared_ptr<RewardsVotes> rewards_votes, secret_t node_sk, vrf_sk_t vrf_sk)
    : db_(db),
      next_votes_manager_(next_votes_mgr),
      pbft_chain_(pbft_chain),
      vote_mgr_(vote_mgr),
      dag_mgr_(dag_mgr),
      dag_blk_mgr_(dag_blk_mgr),
      trx_mgr_(trx_mgr),
      final_chain_(final_chain),
      rewards_votes_(std::move(rewards_votes)),
      node_addr_(node_addr),
      node_sk_(node_sk),
      vrf_sk_(vrf_sk),
      LAMBDA_ms_MIN(conf.lambda_ms_min),
      COMMITTEE_SIZE(conf.committee_size),
      DAG_BLOCKS_SIZE(conf.dag_blocks_size),
      GHOST_PATH_MOVE_BACK(conf.ghost_path_move_back),
      RUN_COUNT_VOTES(conf.run_count_votes),
      dag_genesis_(genesis) {
  LOG_OBJECTS_CREATE("PBFT_MGR");
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  std::vector<blk_hash_t> ghost;
  dag_mgr_->getGhostPath(dag_genesis_, ghost);
  while (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. DAG initialization has not done. Sleep 100ms";
    thisThreadSleepForMilliSeconds(100);
  }
  LOG(log_dg_) << "PBFT start at GHOST size " << ghost.size() << ", the last of DAG blocks is " << ghost.back();
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
       period <= curr_period;  //
       ++period) {
    auto pbft_block = db_->getPbftBlock(period);
    if (!pbft_block.has_value()) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block in period " << period << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    if (pbft_block->getPeriod() != period) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << pbft_block->getBlockHash() << " has different period "
                   << pbft_block->getPeriod() << " in block data than in block order db: " << period;
      assert(false);
    }

    finalize_(SyncBlock(db_->getPeriodDataRaw(period)), period == curr_period);
  }

  // Initialize PBFT status
  initialState_();

  continuousOperation_();
}

// Only to be used for tests...
void PbftManager::resume() {
  if (!stopped_.load()) daemon_->join();
  stopped_ = false;

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
void PbftManager::setMaxWaitForSoftVotedBlock_ms(uint64_t wait_ms) {
  max_wait_for_soft_voted_block_steps_ms_ = wait_ms;
}

// Only to be used for tests...
void PbftManager::setMaxWaitForNextVotedBlock_ms(uint64_t wait_ms) {
  max_wait_for_next_voted_block_steps_ms_ = wait_ms;
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

  daemon_ = std::make_unique<std::thread>([this]() { doNextState_(); });
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

uint64_t PbftManager::getPbftRound() const { return round_; }

uint64_t PbftManager::getPbftStep() const { return step_; }

void PbftManager::setPbftRound(uint64_t const round) {
  db_->savePbftMgrField(PbftMgrRoundStep::PbftRound, round);
  round_ = round;
}

size_t PbftManager::getSortitionThreshold() const { return sortition_threshold_; }

size_t PbftManager::getPreviousPbftPeriodSortitionThreshold() const {
  return previous_pbft_period_sortition_threshold_;
}

size_t PbftManager::getTwoTPlusOne() const { return TWO_T_PLUS_ONE; }

// Notice: Test purpose
void PbftManager::setSortitionThreshold(size_t const sortition_threshold) {
  sortition_threshold_ = sortition_threshold;
}

void PbftManager::updateDposState_() {
  dpos_period_ = pbft_chain_->getPbftChainSize();
  do {
    try {
      size_t tmp_dpos_votes_count = dpos_votes_count_;

      dpos_votes_count_ = final_chain_->dpos_eligible_total_vote_count(dpos_period_);
      weighted_votes_count_ = final_chain_->dpos_eligible_vote_count(dpos_period_, node_addr_);

      // Save current dpos_votes_count_
      previous_pbft_period_dpos_votes_count_ = tmp_dpos_votes_count;
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

size_t PbftManager::getPreviousPbftPeriodDposTotalVotesCount() const {
  return previous_pbft_period_dpos_votes_count_.load();
}

size_t PbftManager::getDposWeightedVotesCount() const { return weighted_votes_count_.load(); }

size_t PbftManager::dposEligibleVoteCount_(addr_t const &addr) {
  try {
    return final_chain_->dpos_eligible_vote_count(dpos_period_, addr);
  } catch (state_api::ErrFutureBlock &c) {
    LOG(log_er_) << c.what() << ". Period " << dpos_period_ << " is too far ahead of DPOS";
    return 0;
  }
}

// Only used by RPC call
uint64_t PbftManager::getVoteWeight(PbftVoteTypes type, uint64_t round, size_t step) const {
  VrfPbftSortition vrf_sortition(vrf_sk_, {type, round, step});
  return vrf_sortition.getBinominalDistribution(weighted_votes_count_, getDposTotalVotesCount(), sortition_threshold_);
}

void PbftManager::setPbftStep(size_t const pbft_step) {
  last_step_ = step_;
  db_->savePbftMgrField(PbftMgrRoundStep::PbftStep, pbft_step);
  step_ = pbft_step;

  if (step_ > MAX_STEPS && LAMBDA_backoff_multiple < 8) {
    // Note: We calculate the lambda for a step independently of prior steps
    //       in case missed earlier steps.
    // std::uniform_int_distribution<u_long> distribution(0, step_ - MAX_STEPS);
    // auto lambda_random_count = distribution(random_engine_);
    // LAMBDA_backoff_multiple = 2 * LAMBDA_backoff_multiple;
    // LAMBDA_ms = LAMBDA_ms_MIN * (LAMBDA_backoff_multiple + lambda_random_count);
    // LOG(log_si_) << "Surpassed max steps, exponentially backing off lambda to " << LAMBDA_ms << " ms in round "
    //             << getPbftRound() << ", step " << step_;
  } else {
    LAMBDA_ms = LAMBDA_ms_MIN;
    LAMBDA_backoff_multiple = 1;
  }
}

void PbftManager::resetStep_() {
  last_step_ = step_;
  step_ = 1;
  startingStepInRound_ = 1;

  LAMBDA_ms = LAMBDA_ms_MIN;
  LAMBDA_backoff_multiple = 1;
}

bool PbftManager::resetRound_() {
  // Jump to round
  auto consensus_pbft_round = vote_mgr_->roundDeterminedFromVotes(TWO_T_PLUS_ONE);
  // current round
  auto round = getPbftRound();

  if (consensus_pbft_round <= round) {
    return false;
  }

  LOG(log_nf_) << "Determined round from votes: " << consensus_pbft_round;
  round_clock_initial_datetime_ = now_;
  // Update current round and reset step to 1
  round_ = consensus_pbft_round;
  resetStep_();
  state_ = value_proposal_state;

  LOG(log_dg_) << "Advancing clock to pbft round " << consensus_pbft_round << ", step 1, and resetting clock.";

  const auto previoud_round = consensus_pbft_round - 1;

  auto next_votes = next_votes_manager_->getNextVotes();

  // Update in DB first
  auto batch = db_->createWriteBatch();

  // Update PBFT round and reset step to 1
  db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftRound, consensus_pbft_round, batch);
  db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftStep, 1, batch);

  db_->addPbft2TPlus1ToBatch(previoud_round, TWO_T_PLUS_ONE, batch);
  db_->addNextVotesToBatch(previoud_round, next_votes, batch);
  if (round > 1) {
    // Cleanup old previoud round next votes
    db_->removeNextVotesToBatch(round - 1, batch);
  }

  db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold, sortition_threshold_,
                                     batch);
  db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, dpos_period_.load(), batch);
  db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount,
                                     getDposTotalVotesCount(), batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::ExecutedInRound, false, batch);
  db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::OwnStartingValueInRound, NULL_BLOCK_HASH, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::SoftVotedBlockInRound, false, batch);
  db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound, NULL_BLOCK_HASH, batch);
  if (soft_voted_block_for_this_round_.second && soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH) {
    db_->removeSoftVotesToBatch(round, batch);
  }
  db_->commitWriteBatch(batch);

  have_executed_this_round_ = false;
  should_have_cert_voted_in_this_round_ = false;
  // reset starting value to NULL_BLOCK_HASH
  own_starting_value_for_round_ = NULL_BLOCK_HASH;
  // reset next voted value since start a new round
  next_voted_null_block_hash_ = false;
  next_voted_soft_value_ = false;
  polling_state_print_log_ = true;

  // Key thing is to set .second to false to mark that we have not
  // identified a soft voted block in the new upcoming round...
  soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, false);

  // Move to a new round, cleanup previous round votes
  vote_mgr_->cleanupVotes(consensus_pbft_round);

  if (executed_pbft_block_) {
    vote_mgr_->removeVerifiedVotes();
    updateDposState_();
    // reset sortition_threshold and TWO_T_PLUS_ONE
    updateTwoTPlusOneAndThreshold_();
    db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
    executed_pbft_block_ = false;
  }

  LAMBDA_ms = LAMBDA_ms_MIN;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();

  // Restart while loop...
  return true;
}

void PbftManager::sleep_() {
  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
  LOG(log_tr_) << "elapsed time in round(ms): " << elapsed_time_in_round_ms_;
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

void PbftManager::initialState_() {
  // Initial PBFT state

  // Time constants...
  LAMBDA_ms = LAMBDA_ms_MIN;
  max_wait_for_soft_voted_block_steps_ms_ = MAX_WAIT_FOR_SOFT_VOTED_BLOCK_STEPS * 2 * LAMBDA_ms;
  max_wait_for_next_voted_block_steps_ms_ = MAX_WAIT_FOR_NEXT_VOTED_BLOCK_STEPS * 2 * LAMBDA_ms;

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
  setPbftRound(round);

  if (round > 1) {
    // Get next votes for previous round from DB
    auto next_votes_in_previous_round = db_->getNextVotes(round - 1);
    if (next_votes_in_previous_round.empty()) {
      LOG(log_er_) << "Cannot get any next votes in previous round " << round - 1 << ". Currrent round " << round
                   << " step " << step;
      assert(false);
    }
    auto previous_round_2t_plus_1 = db_->getPbft2TPlus1(round - 1);
    if (previous_round_2t_plus_1 == 0) {
      LOG(log_er_) << "Cannot get PBFT 2t+1 in previous round " << round - 1 << ". Current round " << round << " step "
                   << step;
      assert(false);
    }
    next_votes_manager_->updateNextVotes(next_votes_in_previous_round, previous_round_2t_plus_1);
  }

  previous_round_next_voted_value_ = next_votes_manager_->getVotedValue();
  previous_round_next_voted_null_block_hash_ = next_votes_manager_->haveEnoughVotesForNullBlockHash();

  LOG(log_nf_) << "Node initialize at round " << round << " step " << step
               << ". Previous round has enough next votes for NULL_BLOCK_HASH: " << std::boolalpha
               << next_votes_manager_->haveEnoughVotesForNullBlockHash() << ", voted value "
               << previous_round_next_voted_value_ << ", next votes size in previous round is "
               << next_votes_manager_->getNextVotesWeight();

  // Initial last sync request
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  auto own_starting_value = db_->getPbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound);
  if (own_starting_value) {
    // From DB
    own_starting_value_for_round_ = *own_starting_value;
  } else {
    // Default value
    own_starting_value_for_round_ = NULL_BLOCK_HASH;
  }

  auto soft_voted_block_hash = db_->getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound);
  auto soft_voted_block = db_->getPbftMgrStatus(PbftMgrStatus::SoftVotedBlockInRound);

  if (soft_voted_block_hash) {
    // From DB
    soft_voted_block_for_this_round_ = std::make_pair(*soft_voted_block_hash, soft_voted_block);
    for (const auto &vote : db_->getSoftVotes(round)) {
      if (vote->getBlockHash() == *soft_voted_block_hash) {
        vote_mgr_->addVerifiedVote(vote);
      }
    }
  } else {
    // Default value
    soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, soft_voted_block);
  }

  // Used for detecting timeout due to malicious node
  // failing to gossip PBFT block or DAG blocks
  // Requires that soft_voted_block_for_this_round_ already be initialized from db
  initializeVotedValueTimeouts_();

  auto last_cert_voted_value = db_->getPbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue);
  if (last_cert_voted_value) {
    last_cert_voted_value_ = *last_cert_voted_value;
    LOG(log_nf_) << "Initialize last cert voted value " << last_cert_voted_value_;
  }

  executed_pbft_block_ = db_->getPbftMgrStatus(PbftMgrStatus::ExecutedBlock);
  have_executed_this_round_ = db_->getPbftMgrStatus(PbftMgrStatus::ExecutedInRound);
  next_voted_soft_value_ = db_->getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue);
  next_voted_null_block_hash_ = db_->getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash);

  round_clock_initial_datetime_ = std::chrono::system_clock::now();
  current_step_clock_initial_datetime_ = round_clock_initial_datetime_;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  next_step_time_ms_ = 0;

  updateDposState_();
  // Initialize TWO_T_PLUS_ONE and sortition_threshold
  updateTwoTPlusOneAndThreshold_();
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
  LOG(log_tr_) << "next step time(ms): " << next_step_time_ms_;
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
               << " | next_voted_soft_value_ = " << next_voted_soft_value_
               << " soft block = " << soft_voted_block_for_this_round_.first
               << " next_voted_null_block_hash_ = " << next_voted_null_block_hash_
               << " last_soft_voted_value_ = " << last_soft_voted_value_
               << " last_cert_voted_value_ = " << last_cert_voted_value_
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
  next_step_time_ms_ = (1 + step_ - startingStepInRound_) * LAMBDA_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

bool PbftManager::stateOperations_() {
  pushSyncedPbftBlocksIntoChain_();

  checkPreviousRoundNextVotedValueChange_();

  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();

  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT current round is " << round;
  LOG(log_tr_) << "PBFT current step is " << step_;

  // Periodically verify unverified votes
  // TODO: sending dposEligibleVoteCount_ as a parameter is not needed here and was added probably just because of
  // single
  //       test that is calling verifyVotes with some dummy lambda function... Very bad design if we adjust live code
  //       because of tests...
  vote_mgr_->verifyVotes(round, sortition_threshold_, getDposTotalVotesCount(),
                         [this](auto const &addr) { return dposEligibleVoteCount_(addr); });

  // CHECK IF WE HAVE RECEIVED 2t+1 CERT VOTES FOR A BLOCK IN OUR CURRENT
  // ROUND.  IF WE HAVE THEN WE EXECUTE THE BLOCK
  // ONLY CHECK IF HAVE *NOT* YET EXECUTED THIS ROUND...
  if (state_ == certify_state && !have_executed_this_round_) {
    auto voted_block_hash_with_cert_votes = vote_mgr_->getVotesBundleByRoundAndStep(round, 3, TWO_T_PLUS_ONE);
    if (voted_block_hash_with_cert_votes.enough) {
      LOG(log_dg_) << "PBFT block " << voted_block_hash_with_cert_votes.voted_block_hash << " has enough cert votes";

      // put pbft block into chain
      const auto vote_size = voted_block_hash_with_cert_votes.votes.size();
      if (pushCertVotedPbftBlockIntoChain_(voted_block_hash_with_cert_votes.voted_block_hash,
                                           std::move(voted_block_hash_with_cert_votes.votes))) {
        db_->savePbftMgrStatus(PbftMgrStatus::ExecutedInRound, true);
        have_executed_this_round_ = true;

        LOG(log_nf_) << "Write " << vote_size << " cert votes ... in round " << round;

        duration_ = std::chrono::system_clock::now() - now_;
        auto execute_trxs_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
        LOG(log_dg_) << "Pushing PBFT block and Execution spent " << execute_trxs_in_ms << " ms. in round " << round;

        // Restart while loop
        return true;
      }
    }
  }

  return resetRound_();
}

bool PbftManager::updateSoftVotedBlockForThisRound_() {
  if (!soft_voted_block_for_this_round_.second) {
    auto round = getPbftRound();

    auto voted_block_hash_with_soft_votes = vote_mgr_->getVotesBundleByRoundAndStep(round, 2, TWO_T_PLUS_ONE);

    auto batch = db_->createWriteBatch();
    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound,
                                     voted_block_hash_with_soft_votes.voted_block_hash, batch);
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::SoftVotedBlockInRound, voted_block_hash_with_soft_votes.enough, batch);
    if (voted_block_hash_with_soft_votes.enough &&
        voted_block_hash_with_soft_votes.voted_block_hash != NULL_BLOCK_HASH) {
      db_->addSoftVotesToBatch(round, voted_block_hash_with_soft_votes.votes, batch);
    }
    db_->commitWriteBatch(batch);

    soft_voted_block_for_this_round_ =
        std::make_pair(voted_block_hash_with_soft_votes.voted_block_hash, voted_block_hash_with_soft_votes.enough);

    if (soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH && soft_voted_block_for_this_round_.second) {
      // Have enough soft votes for a value other other than NULL BLOCK HASH...

      if (state_ == finish_polling_state) {
        LOG(log_dg_) << "Node has seen enough soft votes voted at " << soft_voted_block_for_this_round_.first
                     << ", regossip soft votes. In round " << round;
        if (auto net = network_.lock()) {
          net->onNewPbftVotes(move(voted_block_hash_with_soft_votes.votes));
        }
      }

      return true;
    }
  }
  return false;
}

void PbftManager::initializeVotedValueTimeouts_() {
  time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
  time_began_waiting_soft_voted_block_ = std::chrono::system_clock::now();

  // Concern: Requires that soft_voted_block_for_this_round_ already be initialized from db
  if (soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH && soft_voted_block_for_this_round_.second) {
    last_soft_voted_value_ = soft_voted_block_for_this_round_.first;
  }
}

// Only for test
void PbftManager::setLastSoftVotedValue(blk_hash_t soft_voted_value) { updateLastSoftVotedValue_(soft_voted_value); }

void PbftManager::updateLastSoftVotedValue_(blk_hash_t const new_soft_voted_value) {
  if (new_soft_voted_value != last_soft_voted_value_) {
    time_began_waiting_soft_voted_block_ = std::chrono::system_clock::now();
  }
  last_soft_voted_value_ = new_soft_voted_value;
}

void PbftManager::checkPreviousRoundNextVotedValueChange_() {
  auto previous_round_next_voted_value = next_votes_manager_->getVotedValue();
  auto previous_round_next_voted_null_block_hash = next_votes_manager_->haveEnoughVotesForNullBlockHash();

  if (previous_round_next_voted_value != previous_round_next_voted_value_) {
    time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
    previous_round_next_voted_value_ = previous_round_next_voted_value;
  } else if (previous_round_next_voted_null_block_hash != previous_round_next_voted_null_block_hash_) {
    time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
    previous_round_next_voted_null_block_hash_ = previous_round_next_voted_null_block_hash;
  }
}

void PbftManager::proposeBlock_() {
  // Value Proposal
  auto round = getPbftRound();

  LOG(log_tr_) << "PBFT value proposal state in round " << round;
  if (round > 1) {
    if (next_votes_manager_->haveEnoughVotesForNullBlockHash()) {
      LOG(log_nf_) << "Previous round " << round - 1 << " next voted block is NULL_BLOCK_HASH";
    } else if (previous_round_next_voted_value_ != NULL_BLOCK_HASH) {
      LOG(log_nf_) << "Previous round " << round - 1 << " next voted block is " << previous_round_next_voted_value_;
    } else {
      LOG(log_er_) << "Previous round " << round - 1 << " doesn't have enough next votes";
      assert(false);
    }
  }

  if (round == 1) {
    auto place_votes = placeVote_(own_starting_value_for_round_, propose_vote_type, round, step_);
    if (place_votes) {
      LOG(log_nf_) << "Proposing " << place_votes << " values of NULL_BLOCK_HASH " << NULL_BLOCK_HASH
                   << " for round 1 by protocol";
    }
  } else if (round >= 2 && giveUpNextVotedBlock_()) {
    // PBFT block only be proposed once in one period
    if (!proposed_block_hash_.second || proposed_block_hash_.first == NULL_BLOCK_HASH) {
      // Propose value...
      proposed_block_hash_ = proposeMyPbftBlock_();
    }
    if (proposed_block_hash_.second) {
      db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, proposed_block_hash_.first);
      own_starting_value_for_round_ = proposed_block_hash_.first;

      auto place_votes = placeVote_(own_starting_value_for_round_, propose_vote_type, round, step_);
      if (place_votes) {
        LOG(log_nf_) << "Proposing " << place_votes << " own starting value " << own_starting_value_for_round_
                     << " for round " << round;
      }
    }
  } else if (round >= 2 && previous_round_next_voted_value_ != NULL_BLOCK_HASH) {
    db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, previous_round_next_voted_value_);
    own_starting_value_for_round_ = previous_round_next_voted_value_;

    auto pbft_block = pbft_chain_->getUnverifiedPbftBlock(own_starting_value_for_round_);
    if (!pbft_block) {
      LOG(log_dg_) << "Can't get proposal block " << own_starting_value_for_round_ << " in unverified queue";
      pbft_block = db_->getPbftCertVotedBlock(own_starting_value_for_round_);
      if (!pbft_block) {
        LOG(log_dg_) << "Can't get proposal block " << own_starting_value_for_round_ << " in database";
      }
    }
    if (pbft_block) {
      // place vote
      auto place_votes = placeVote_(own_starting_value_for_round_, propose_vote_type, round, step_);
      if (place_votes) {
        LOG(log_nf_) << "Rebroadcast next voted block " << own_starting_value_for_round_ << ", and propose "
                     << place_votes << " votes "
                     << " from previous round. In round " << round;
        // broadcast pbft block
        if (auto net = network_.lock()) {
          net->onNewPbftBlock(pbft_block);
        }
      }
    }
  }
}

void PbftManager::identifyBlock_() {
  // The Filtering Step
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT filtering state in round " << round;

  if (round == 1 || (round >= 2 && giveUpNextVotedBlock_())) {
    // Identity leader
    std::pair<blk_hash_t, bool> leader_block = identifyLeaderBlock_();
    if (leader_block.second) {
      db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, leader_block.first);
      own_starting_value_for_round_ = leader_block.first;
      LOG(log_dg_) << "Identify leader block " << leader_block.first << " at round " << round;

      auto place_votes = placeVote_(leader_block.first, soft_vote_type, round, step_);

      updateLastSoftVotedValue_(leader_block.first);

      if (place_votes) {
        LOG(log_nf_) << "Soft votes " << place_votes << " voting block " << leader_block.first << " at round " << round;
      }
    }
  } else if (round >= 2 && previous_round_next_voted_value_ != NULL_BLOCK_HASH) {
    auto place_votes = placeVote_(previous_round_next_voted_value_, soft_vote_type, round, step_);

    // Generally this value will either be the same as last soft voted value from previous round
    // but a node could have observed a previous round next voted value that differs from what they
    // soft voted.
    updateLastSoftVotedValue_(previous_round_next_voted_value_);

    if (place_votes) {
      LOG(log_nf_) << "Soft votes " << place_votes << " voting block " << previous_round_next_voted_value_
                   << " from previous round. In round " << round;
    }
  }
}

void PbftManager::certifyBlock_() {
  // The Certifying Step
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT certifying state in round " << round;

  go_finish_state_ = elapsed_time_in_round_ms_ > 4 * LAMBDA_ms - POLLING_INTERVAL_ms;

  if (elapsed_time_in_round_ms_ < 2 * LAMBDA_ms) {
    // Should not happen, add log here for safety checking
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only " << elapsed_time_in_round_ms_ << " (ms) in round "
                 << round;
  } else if (go_finish_state_) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in round " << round;
  } else if (!should_have_cert_voted_in_this_round_) {
    LOG(log_tr_) << "In step 3";

    if (updateSoftVotedBlockForThisRound_() &&
        comparePbftBlockScheduleWithDAGblocks_(soft_voted_block_for_this_round_.first)) {
      LOG(log_tr_) << "Finished comparePbftBlockScheduleWithDAGblocks_";

      // NOTE: If we have already executed this round then block won't be found in unverified queue...
      bool executed_soft_voted_block_for_this_round = false;
      if (have_executed_this_round_) {
        LOG(log_tr_) << "Have already executed before certifying in step 3 in round " << round;
        auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
        if (last_pbft_block_hash == soft_voted_block_for_this_round_.first) {
          LOG(log_tr_) << "Having executed, last block in chain is the soft voted block in round " << round;
          executed_soft_voted_block_for_this_round = true;
        }
      }

      bool unverified_soft_vote_block_for_this_round_is_valid = false;
      if (!executed_soft_voted_block_for_this_round) {
        auto block = pbft_chain_->getUnverifiedPbftBlock(soft_voted_block_for_this_round_.first);
        if (block && pbft_chain_->checkPbftBlockValidation(*block)) {
          unverified_soft_vote_block_for_this_round_is_valid = true;
        } else {
          if (!block) {
            LOG(log_er_) << "Cannot find the unverified pbft block " << soft_voted_block_for_this_round_.first
                         << " in round " << round << " step 3";
          }
          syncPbftChainFromPeers_(invalid_soft_voted_block, soft_voted_block_for_this_round_.first);
        }
      }

      if (executed_soft_voted_block_for_this_round || unverified_soft_vote_block_for_this_round_is_valid) {
        // comparePbftBlockScheduleWithDAGblocks_ has checked the cert voted block exist
        last_cert_voted_value_ = soft_voted_block_for_this_round_.first;
        auto cert_voted_block = getUnfinalizedBlock_(last_cert_voted_value_);

        auto batch = db_->createWriteBatch();
        db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::LastCertVotedValue, last_cert_voted_value_, batch);
        db_->addPbftCertVotedBlockToBatch(*cert_voted_block, batch);
        db_->commitWriteBatch(batch);

        should_have_cert_voted_in_this_round_ = true;

        // generate cert vote
        auto place_votes = placeVote_(soft_voted_block_for_this_round_.first, cert_vote_type, round, step_);
        if (place_votes) {
          LOG(log_nf_) << "Cert votes " << place_votes << " voting " << soft_voted_block_for_this_round_.first
                       << " in round " << round;
        }
      }
    }
  }
}

void PbftManager::firstFinish_() {
  // Even number steps from 4 are in first finish

  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT first finishing state at step " << step_ << " in round " << round;

  if (last_cert_voted_value_ != NULL_BLOCK_HASH) {
    auto place_votes = placeVote_(last_cert_voted_value_, next_vote_type, round, step_);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting cert voted value " << last_cert_voted_value_
                   << " for round " << round << " , step " << step_;
    }
    // Re-broadcast pbft block in case some nodes do not have it
    if (step_ % 20 == 0) {
      auto pbft_block = db_->getPbftCertVotedBlock(last_cert_voted_value_);
      assert(pbft_block);
      if (auto net = network_.lock()) {
        net->onNewPbftBlock(pbft_block);
      }
    }
  } else {
    // We only want to give up soft voted value IF:
    // 1) haven't cert voted it
    // 2) we are looking at value that was next voted in previous round
    // 3) we don't have the block or if have block it can't be cert voted (yet)
    bool giveUpSoftVotedBlockInFirstFinish = last_cert_voted_value_ == NULL_BLOCK_HASH &&
                                             own_starting_value_for_round_ == previous_round_next_voted_value_ &&
                                             giveUpSoftVotedBlock_() &&
                                             !comparePbftBlockScheduleWithDAGblocks_(own_starting_value_for_round_);

    if (round >= 2 && (giveUpNextVotedBlock_() || giveUpSoftVotedBlockInFirstFinish)) {
      auto place_votes = placeVote_(NULL_BLOCK_HASH, next_vote_type, round, step_);
      if (place_votes) {
        LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step "
                     << step_;
      }
    } else {
      if (own_starting_value_for_round_ != previous_round_next_voted_value_ &&
          previous_round_next_voted_value_ != NULL_BLOCK_HASH &&
          !pbft_chain_->findPbftBlockInChain(previous_round_next_voted_value_)) {
        if (own_starting_value_for_round_ == NULL_BLOCK_HASH) {
          db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, previous_round_next_voted_value_);
          own_starting_value_for_round_ = previous_round_next_voted_value_;
          LOG(log_dg_) << "Updating own starting value of NULL BLOCK HASH to previous round next voted value of "
                       << previous_round_next_voted_value_;
        } else if (comparePbftBlockScheduleWithDAGblocks_(previous_round_next_voted_value_)) {
          // Check if we have received the previous round next voted value and its a viable value...
          // IF it is viable then reset own starting value to it...
          db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, previous_round_next_voted_value_);
          LOG(log_dg_) << "Updating own starting value of " << own_starting_value_for_round_
                       << " to previous round next voted value of " << previous_round_next_voted_value_;
          own_starting_value_for_round_ = previous_round_next_voted_value_;
        }
      }

      auto place_votes = placeVote_(own_starting_value_for_round_, next_vote_type, round, step_);
      if (place_votes) {
        LOG(log_nf_) << "Next votes " << place_votes << " voting nodes own starting value "
                     << own_starting_value_for_round_ << " for round " << round << ", at step " << step_;
      }
    }
  }
}

void PbftManager::secondFinish_() {
  // Odd number steps from 5 are in second finish
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT second finishing state at step " << step_ << " in round " << round;
  assert(step_ >= startingStepInRound_);
  auto end_time_for_step = (2 + step_ - startingStepInRound_) * LAMBDA_ms - POLLING_INTERVAL_ms;

  updateSoftVotedBlockForThisRound_();

  // We only want to give up soft voted value IF:
  // 1) haven't cert voted it
  // 2) we are looking at value that was next voted in previous round
  // 3) we don't have the block or if have block it can't be cert voted (yet)
  bool giveUpSoftVotedBlockInSecondFinish =
      last_cert_voted_value_ == NULL_BLOCK_HASH && last_soft_voted_value_ == previous_round_next_voted_value_ &&
      giveUpSoftVotedBlock_() && !comparePbftBlockScheduleWithDAGblocks_(soft_voted_block_for_this_round_.first);

  if (!next_voted_soft_value_ && soft_voted_block_for_this_round_.second &&
      soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH && !giveUpSoftVotedBlockInSecondFinish) {
    auto place_votes = placeVote_(soft_voted_block_for_this_round_.first, next_vote_type, round, step_);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting " << soft_voted_block_for_this_round_.first
                   << " for round " << round << ", at step " << step_;

      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
      next_voted_soft_value_ = true;
    }
  }

  if (!next_voted_null_block_hash_ && round >= 2 && (giveUpSoftVotedBlockInSecondFinish || giveUpNextVotedBlock_())) {
    auto place_votes = placeVote_(NULL_BLOCK_HASH, next_vote_type, round, step_);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step " << step_;

      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      next_voted_null_block_hash_ = true;
    }
  }

  if (step_ > MAX_STEPS && (step_ - MAX_STEPS - 2) % 100 == 0) {
    syncPbftChainFromPeers_(exceeded_max_steps, NULL_BLOCK_HASH);
  }

  if (step_ > MAX_STEPS && (step_ - MAX_STEPS - 2) % 100 == 0 && !broadcastAlreadyThisStep_()) {
    LOG(log_dg_) << "Node " << node_addr_ << " broadcast next votes for previous round. In round " << round << " step "
                 << step_;
    if (auto net = network_.lock()) {
      net->broadcastPreviousRoundNextVotesBundle();
    }
    pbft_round_last_broadcast_ = round;
    pbft_step_last_broadcast_ = step_;
  }

  loop_back_finish_state_ = elapsed_time_in_round_ms_ > end_time_for_step;
}

std::shared_ptr<Vote> PbftManager::generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t round,
                                                size_t step) {
  // sortition proof
  VrfPbftSortition vrf_sortition(vrf_sk_, {type, round, step});
  return std::make_shared<Vote>(node_sk_, std::move(vrf_sortition), blockhash);
}

size_t PbftManager::placeVote_(taraxa::blk_hash_t const &blockhash, PbftVoteTypes vote_type, uint64_t round,
                               size_t step) {
  if (!weighted_votes_count_) {
    // No delegation
    return 0;
  }

  auto dpos_weighted_votes_count = weighted_votes_count_.load();
  if (step == 1) {
    // For proposal vote, only use 1 weight for VRF sortition
    dpos_weighted_votes_count = 1;
  }

  auto vote = generateVote(blockhash, vote_type, round, step);
  const auto weight = vote->calculateWeight(dpos_weighted_votes_count, getDposTotalVotesCount(), sortition_threshold_);
  if (weight) {
    db_->saveVerifiedVote(vote);
    vote_mgr_->addVerifiedVote(vote);
    if (auto net = network_.lock()) {
      net->onNewPbftVotes({std::move(vote)});
    }
  }
  return weight;
}

blk_hash_t PbftManager::calculateOrderHash(std::vector<blk_hash_t> const &dag_block_hashes,
                                           std::vector<trx_hash_t> const &trx_hashes) {
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

blk_hash_t PbftManager::calculateOrderHash(std::vector<DagBlock> const &dag_blocks,
                                           std::vector<Transaction> const &trxs) {
  dev::RLPStream order_stream(2);
  order_stream.appendList(dag_blocks.size());
  for (auto const &blk : dag_blocks) {
    order_stream << blk.getHash();
  }
  order_stream.appendList(trxs.size());
  for (auto const &trx : trxs) {
    order_stream << trx.getHash();
  }
  return dev::sha3(order_stream.out());
}

std::pair<blk_hash_t, bool> PbftManager::proposeMyPbftBlock_() {
  auto round = getPbftRound();
  // In propose block, only use one vote for VRF sortition
  VrfPbftSortition vrf_sortition(vrf_sk_, {propose_vote_type, round, step_});
  if (weighted_votes_count_ == 0 ||
      !vrf_sortition.getBinominalDistribution(1, getDposTotalVotesCount(), sortition_threshold_)) {
    return std::make_pair(NULL_BLOCK_HASH, false);
  }

  LOG(log_dg_) << "Into propose PBFT block";
  blk_hash_t last_period_dag_anchor_block_hash;
  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    last_period_dag_anchor_block_hash = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash).getPivotDagBlockHash();
  } else {
    // First PBFT pivot block
    last_period_dag_anchor_block_hash = dag_genesis_;
  }

  std::vector<blk_hash_t> ghost;
  dag_mgr_->getGhostPath(last_period_dag_anchor_block_hash, ghost);
  LOG(log_dg_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at lease include the last period dag anchor block
  if (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. No new DAG blocks generated, PBFT "
                    "propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
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
  if (dag_block_hash == dag_genesis_) {
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis " << dag_block_hash
                 << " PBFT propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // compare with last dag block hash. If they are same, which means no new
  // dag blocks generated since last round. In that case PBFT proposer should
  // propose NULL BLOCK HASH as their value and not produce a new block. In
  // practice this should never happen
  if (dag_block_hash == last_period_dag_anchor_block_hash) {
    LOG(log_dg_) << "Last period DAG anchor block hash " << dag_block_hash
                 << " No new DAG blocks generated, PBFT propose NULL_BLOCK_HASH";
    LOG(log_dg_) << "Ghost: " << ghost;
    return std::make_pair(NULL_BLOCK_HASH, true);
  }

  uint64_t propose_pbft_period = pbft_chain_->getPbftChainSize() + 1;
  addr_t beneficiary = node_addr_;

  // get DAG block and transaction order
  auto dag_block_order = dag_mgr_->getDagBlockOrder(dag_block_hash, propose_pbft_period);
  if (dag_block_order.empty()) {
    LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlockOrder failed in propose";
    assert(false);
  }
  std::vector<trx_hash_t> non_finalized_transactions;
  std::unordered_set<trx_hash_t> trx_hashes_set;
  std::vector<trx_hash_t> trx_hashes;
  for (auto const &blk_hash : dag_block_order) {
    auto dag_blk = dag_blk_mgr_->getDagBlock(blk_hash);
    if (!dag_blk) {
      LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlock failed in propose for block "
                   << blk_hash;
      assert(false);
    }
    for (auto const &trx_hash : dag_blk->getTrxs()) {
      if (trx_hashes_set.emplace(trx_hash).second) {
        trx_hashes.emplace_back(trx_hash);
      }
    }
  }
  auto trx_finalized = db_->transactionsFinalized(trx_hashes);
  for (uint32_t i = 0; i < trx_finalized.size(); i++) {
    if (!trx_finalized[i]) {
      non_finalized_transactions.emplace_back(trx_hashes[i]);
    }
  }

  auto order_hash = calculateOrderHash(dag_block_order, non_finalized_transactions);

  // generate generate pbft block
  auto pbft_block = std::make_shared<PbftBlock>(last_pbft_block_hash, dag_block_hash, order_hash, propose_pbft_period,
                                                beneficiary, node_sk_);

  LOG(log_nf_) << "Proposed Pbft block: " << pbft_block->getBlockHash() << ". Order hash:" << order_hash
               << ". DAG order for proposed block" << dag_block_order << ". Transaction order for proposed block"
               << non_finalized_transactions;

  // push pbft block
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
  // broadcast pbft block
  if (auto net = network_.lock()) {
    net->onNewPbftBlock(pbft_block);
  }

  LOG(log_dg_) << node_addr_ << " propose PBFT block succussful! in round: " << round << " in step: " << step_
               << " PBFT block: " << pbft_block;
  return std::make_pair(pbft_block->getBlockHash(), true);
}

std::vector<std::vector<uint>> PbftManager::createMockTrxSchedule(
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>> trx_overlap_table) {
  std::vector<std::vector<uint>> blocks_trx_modes;

  if (!trx_overlap_table) {
    LOG(log_er_) << "Transaction overlap table nullptr, cannot create mock "
                 << "transactions schedule";
    return blocks_trx_modes;
  }

  for (size_t i = 0; i < trx_overlap_table->size(); i++) {
    blk_hash_t &dag_block_hash = (*trx_overlap_table)[i].first;
    auto blk = dag_blk_mgr_->getDagBlock(dag_block_hash);
    if (!blk) {
      LOG(log_er_) << "Cannot create schedule block, DAG block missing " << dag_block_hash;
      continue;
    }

    auto num_trx = blk->getTrxs().size();
    std::vector<uint> block_trx_modes;
    for (size_t j = 0; j < num_trx; j++) {
      if ((*trx_overlap_table)[i].second[j]) {
        // trx sequential mode
        block_trx_modes.emplace_back(1);
      } else {
        // trx invalid mode
        block_trx_modes.emplace_back(0);
      }
    }
    blocks_trx_modes.emplace_back(block_trx_modes);
  }

  return blocks_trx_modes;
}

std::pair<blk_hash_t, bool> PbftManager::identifyLeaderBlock_() {
  auto round = getPbftRound();
  LOG(log_dg_) << "Into identify leader block, in round " << round;

  // Get all proposal votes in the round
  auto votes = vote_mgr_->getProposalVotes(round);

  // each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<vrf_output_t, blk_hash_t>> leader_candidates;

  for (auto const &v : votes) {
    if (v->getRound() == round && v->getType() == propose_vote_type) {
      // We should not pick any null block as leader (proposed when
      // no new blocks found, or maliciously) if others have blocks.
      auto proposed_block_hash = v->getBlockHash();

      // Make sure we don't keep soft voting for soft value we want to give up...
      if (proposed_block_hash == last_soft_voted_value_ && giveUpSoftVotedBlock_()) {
        continue;
      }

      if (round == 1 ||
          (proposed_block_hash != NULL_BLOCK_HASH && !pbft_chain_->findPbftBlockInChain(proposed_block_hash))) {
        leader_candidates.emplace_back(std::make_pair(v->getCredential(), proposed_block_hash));
      }
    }
  }
  if (leader_candidates.empty()) {
    // no eligible leader
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  std::pair<vrf_output_t, blk_hash_t> leader =
      *std::min_element(leader_candidates.begin(), leader_candidates.end(),
                        [](std::pair<vrf_output_t, blk_hash_t> const &i, std::pair<vrf_output_t, blk_hash_t> const &j) {
                          return i.first < j.first;
                        });

  return std::make_pair(leader.second, true);
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return getPbftRound() == pbft_round_last_requested_sync_ && step_ == pbft_step_last_requested_sync_;
}

void PbftManager::syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash) {
  if (stopped_) {
    return;
  }
  if (auto net = network_.lock()) {
    if (syncBlockQueueSize()) {
      LOG(log_tr_) << "PBFT synced queue is still processing so skip syncing. Synced queue size "
                   << syncBlockQueueSize();

      return;
    }

    if (!is_syncing_() && !syncRequestedAlreadyThisStep_()) {
      auto round = getPbftRound();

      bool force = false;

      switch (reason) {
        case missing_dag_blk:
          LOG(log_nf_) << "DAG blocks have not synced yet, anchor block " << relevant_blk_hash
                       << " not present in DAG.";
          // We want to force syncing the DAG...
          force = true;

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
          force = true;
          break;
        default:
          LOG(log_er_) << "Unknown PBFT sync request reason " << reason;
          assert(false);

          if (force) {
            LOG(log_nf_) << "Restarting sync in round " << round << ", step " << step_ << ", and forcing DAG sync";
          } else {
            LOG(log_nf_) << "Restarting sync in round " << round << ", step " << step_;
          }
      }

      net->restartSyncingPbft(force);

      pbft_round_last_requested_sync_ = round;
      pbft_step_last_requested_sync_ = step_;
    }
  }
}

bool PbftManager::broadcastAlreadyThisStep_() const {
  return getPbftRound() == pbft_round_last_broadcast_ && step_ == pbft_step_last_broadcast_;
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(blk_hash_t const &pbft_block_hash) {
  auto pbft_block = getUnfinalizedBlock_(pbft_block_hash);
  if (!pbft_block) {
    return false;
  }

  return comparePbftBlockScheduleWithDAGblocks_(pbft_block);
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(const std::shared_ptr<PbftBlock> &pbft_block) {
  // If cert sync block is already populated with this pbft block, no need to do verification again
  const auto &cached_pbft_block = cert_sync_block_.getPbftBlock();
  if (cached_pbft_block && cached_pbft_block->getBlockHash() == pbft_block->getBlockHash()) {
    return true;
  }

  auto const &anchor_hash = pbft_block->getPivotDagBlockHash();
  auto dag_blocks_order = dag_mgr_->getDagBlockOrder(anchor_hash, pbft_block->getPeriod());

  if (dag_blocks_order.empty()) {
    syncPbftChainFromPeers_(missing_dag_blk, anchor_hash);
    return false;
  }

  cert_sync_block_.clear();

  std::vector<DagBlock> tmp_dag_blocks;
  tmp_dag_blocks.reserve(dag_blocks_order.size());

  std::unordered_set<trx_hash_t> trx_set;
  std::vector<trx_hash_t> transactions_to_query;

  for (auto const &dag_blk_hash : dag_blocks_order) {
    auto dag_block = dag_blk_mgr_->getDagBlock(dag_blk_hash);
    assert(dag_block);
    for (auto const &trx_hash : dag_block->getTrxs()) {
      if (trx_set.insert(trx_hash).second) {
        transactions_to_query.emplace_back(trx_hash);
      }
    }

    tmp_dag_blocks.emplace_back(std::move(*dag_block));
  }

  std::vector<trx_hash_t> non_finalized_transactions;

  auto trx_finalized = db_->transactionsFinalized(transactions_to_query);
  for (uint32_t i = 0; i < trx_finalized.size(); i++) {
    if (!trx_finalized[i]) {
      non_finalized_transactions.emplace_back(transactions_to_query[i]);
    }
  }

  auto calculated_order_hash = calculateOrderHash(dag_blocks_order, non_finalized_transactions);
  if (calculated_order_hash != pbft_block->getOrderHash()) {
    LOG(log_er_) << "Order hash incorrect. Pbft block: " << pbft_block->getBlockHash()
                 << ". Order hash: " << pbft_block->getOrderHash() << " . Calculated hash:" << calculated_order_hash
                 << ". Dag order: " << dag_blocks_order << ". Trx order: " << non_finalized_transactions;
    return false;
  }

  DbStorage::MultiGetQuery db_query(db_);
  db_query.append(DbStorage::Columns::transactions, non_finalized_transactions);
  auto transactions_res = db_query.execute();

  std::vector<Transaction> tmp_txs;
  tmp_txs.reserve(transactions_res.size());
  vec_trx_t tmp_txs_hashes;
  tmp_txs_hashes.reserve(transactions_res.size());

  for (auto const &trx_raw : transactions_res) {
    assert(trx_raw.size() > 0);
    auto tx = Transaction(dev::asBytes(trx_raw));

    tmp_txs_hashes.push_back(tx.getHash());
    tmp_txs.push_back(std::move(tx));
  }

  cert_sync_block_ = SyncBlock(pbft_block, std::move(tmp_dag_blocks), std::move(dag_blocks_order), std::move(tmp_txs),
                               std::move(tmp_txs_hashes));

  return true;
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(taraxa::blk_hash_t const &cert_voted_block_hash,
                                                   std::vector<std::shared_ptr<Vote>> &&cert_votes_for_round) {
  auto pbft_block = getUnfinalizedBlock_(cert_voted_block_hash);
  if (!pbft_block) {
    LOG(log_nf_) << "Can not find the cert voted block hash " << cert_voted_block_hash << " in both pbft queue and DB";
    return false;
  }

  if (!pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
    syncPbftChainFromPeers_(invalid_cert_voted_block, cert_voted_block_hash);
    return false;
  }

  if (!comparePbftBlockScheduleWithDAGblocks_(pbft_block)) {
    LOG(log_nf_) << "DAG has not build up for PBFT block " << cert_voted_block_hash;
    return false;
  }
  cert_sync_block_.setCertVotes(std::move(cert_votes_for_round));

  // TODO: check if cert_sync_block_ has to be created again inside comparePbftBlockScheduleWithDAGblocks_ even
  //  after pushPbftBlock. If yes, shared_ptr to const SyncBlock should be probably used ?
  if (!pushPbftBlock_(std::move(cert_sync_block_))) {
    LOG(log_er_) << "Failed push PBFT block " << pbft_block->getBlockHash() << " into chain";
    return false;
  }

  // cleanup PBFT unverified blocks table
  pbft_chain_->cleanupUnverifiedPbftBlocks(*pbft_block);
  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain_() {
  if (auto net = network_.lock()) {
    auto round = getPbftRound();
    while (syncBlockQueueSize() > 0) {
      auto sync_block = processSyncBlock();
      if (!sync_block.has_value()) {
        continue;
      }

      auto pbft_block_hash = sync_block->getPbftBlock()->getBlockHash();
      LOG(log_nf_) << "Pick pbft block " << pbft_block_hash << " from synced queue in round " << round;

      if (!pushPbftBlock_(std::move(sync_block.value()))) {
        LOG(log_er_) << "Failed push PBFT block " << pbft_block_hash << " into chain";
        break;
      }

      net->setSyncStatePeriod(sync_block.pbft_blk->getPeriod());

      if (executed_pbft_block_) {
        vote_mgr_->removeVerifiedVotes();
        updateDposState_();
        // update sortition_threshold and TWO_T_PLUS_ONE
        updateTwoTPlusOneAndThreshold_();
        db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
        executed_pbft_block_ = false;
      }
    }
  }
}

void PbftManager::finalize_(SyncBlock &&sync_block, bool synchronous_processing) {
  auto anchor_block_hash = sync_block.getPbftBlock()->getPivotDagBlockHash();
  auto result = final_chain_->finalize(
      std::move(sync_block),
      [this, weak_ptr = weak_from_this(), anchor_hash = std::move(anchor_block_hash)](auto const &, auto &batch) {
        // Update proposal period DAG levels map
        auto ptr = weak_ptr.lock();
        if (!ptr) {
          return;  // it was destroyed
        }

        auto anchor = dag_blk_mgr_->getDagBlock(anchor_hash);
        if (!anchor) {
          LOG(log_er_) << "DB corrupted - Cannot find anchor block: " << anchor_hash << " in DB.";
          assert(false);
        }

        auto new_proposal_period_levels_map = dag_blk_mgr_->newProposePeriodDagLevelsMap(anchor->getLevel());
        db_->addProposalPeriodDagLevelsMapToBatch(*new_proposal_period_levels_map, batch);
        auto dpos_current_max_proposal_period = dag_blk_mgr_->getCurrentMaxProposalPeriod();
        db_->addDposProposalPeriodLevelsFieldToBatch(DposProposalPeriodLevelsStatus::MaxProposalPeriod,
                                                     dpos_current_max_proposal_period, batch);
      });
  if (synchronous_processing) {
    result.wait();
  }
}

bool PbftManager::pushPbftBlock_(SyncBlock &&sync_block) {
  const auto &pbft_block = sync_block.getPbftBlock();

  // Not a reference by purpose - pbft_block_hash is used after sync_block is std::moved
  const auto pbft_block_hash = pbft_block->getBlockHash();

  if (db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_nf_) << "PBFT block: " << pbft_block_hash << " in DB already.";
    if (last_cert_voted_value_ == pbft_block_hash) {
      LOG(log_er_) << "Last cert voted value should be NULL_BLOCK_HASH. Block hash " << last_cert_voted_value_
                   << " has been pushed into chain already";
      assert(false);
    }
    return false;
  }

  auto batch = db_->createWriteBatch();

  LOG(log_nf_) << "Storing cert votes of pbft blk " << pbft_block_hash;
  LOG(log_dg_) << "Stored following cert votes:\n" << sync_block.getCertVotes();
  // Update PBFT chain head block
  db_->addPbftHeadToBatch(pbft_chain_->getHeadHash(), pbft_chain_->getJsonStrForBlock(pbft_block_hash), batch);

  db_->savePeriodData(sync_block, batch);

  // Reset last cert voted value to NULL_BLOCK_HASH
  db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::LastCertVotedValue, NULL_BLOCK_HASH, batch);

  // pass pbft with dag blocks and transactions to adjust difficulty
  dag_blk_mgr_->sortitionParamsManager().pbftBlockPushed(sync_block, batch);

  // Commit DB
  db_->commitWriteBatch(batch);

  // Set DAG blocks period
  // TODO: could be passed only SyncBlock -> a lot of refactoring in tests
  dag_mgr_->setDagBlockOrder(pbft_block->getPivotDagBlockHash(), pbft_block->getPeriod(),
                             sync_block.getDagBlocksHashes());

  trx_mgr_->updateFinalizedTransactionsStatus(sync_block.getTransactionsHashes());

  // update PBFT chain size
  pbft_chain_->updatePbftChain(pbft_block_hash);

  last_cert_voted_value_ = NULL_BLOCK_HASH;

  LOG(log_nf_) << "Pushed new PBFT block " << pbft_block_hash << " into chain. Period: " << pbft_block->getPeriod()
               << ", round: " << getPbftRound();

  // Transform cert votes into the set of hashes
  const auto &cert_votes = sync_block.getCertVotes();
  std::unordered_set<vote_hash_t> cert_votes_hashes;
  cert_votes_hashes.reserve(cert_votes.size());
  for (const auto &vote : cert_votes) {
    cert_votes_hashes.insert(vote->getHash());
  }

  // TODO: can sent only sync_block...
  // Reset 2t+1 cert votes in rewards votes(votes that should be rewarded in the next pbft period)
  rewards_votes_->newPbftBlockFinalized(std::move(cert_votes_hashes), pbft_block_hash,
                                        sync_block.getAllUniqueRewardsVotes());

  finalize_(std::move(sync_block));

  // Reset proposed PBFT block hash to False for next pbft block proposal
  proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);
  db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  executed_pbft_block_ = true;

  return true;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  // Save current threshold
  previous_pbft_period_sortition_threshold_ = sortition_threshold_;

  // Update 2t+1 and threshold
  auto dpos_total_votes_count = getDposTotalVotesCount();
  sortition_threshold_ = std::min<size_t>(COMMITTEE_SIZE, dpos_total_votes_count);
  TWO_T_PLUS_ONE = sortition_threshold_ * 2 / 3 + 1;
  LOG(log_nf_) << "Committee size " << COMMITTEE_SIZE << ", DPOS total votes count " << dpos_total_votes_count
               << ". Update 2t+1 " << TWO_T_PLUS_ONE << ", Threshold " << sortition_threshold_;
}

bool PbftManager::giveUpSoftVotedBlock_() {
  if (last_soft_voted_value_ == NULL_BLOCK_HASH) return false;

  auto now = std::chrono::system_clock::now();
  auto soft_voted_block_wait_duration = now - time_began_waiting_soft_voted_block_;
  unsigned long elapsed_wait_soft_voted_block_in_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(soft_voted_block_wait_duration).count();

  auto pbft_block = getUnfinalizedBlock_(previous_round_next_voted_value_);
  if (pbft_block) {
    // Have a block, but is it valid?
    if (!pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      // Received the block, but not valid
      return true;
    }
  }

  if (elapsed_wait_soft_voted_block_in_ms > max_wait_for_soft_voted_block_steps_ms_) {
    LOG(log_dg_) << "Have been waiting " << elapsed_wait_soft_voted_block_in_ms << "ms for soft voted block "
                 << last_soft_voted_value_ << ", giving up on this value.";
    return true;
  } else {
    LOG(log_tr_) << "Have only been waiting " << elapsed_wait_soft_voted_block_in_ms << "ms for soft voted block "
                 << last_soft_voted_value_ << "(after " << max_wait_for_soft_voted_block_steps_ms_
                 << "ms will give up on this value)";
  }

  return false;
}

bool PbftManager::giveUpNextVotedBlock_() {
  auto round = getPbftRound();

  if (last_cert_voted_value_ != NULL_BLOCK_HASH) {
    // Last cert voted value should equal to voted value
    if (polling_state_print_log_) {
      LOG(log_nf_) << "In round " << round << " step " << step_ << ", last cert voted value is "
                   << last_cert_voted_value_;
      polling_state_print_log_ = false;
    }
    return false;
  }

  if (previous_round_next_voted_value_ == NULL_BLOCK_HASH) {
    LOG(log_nf_) << "In round " << round << " step " << step_
                 << ", have received 2t+1 next votes for NULL_BLOCK_HASH for previous round.";
    return true;
  } else if (next_votes_manager_->haveEnoughVotesForNullBlockHash()) {
    LOG(log_nf_)
        << "In round " << round << " step " << step_
        << ", There are 2 voted values in previous round, and have received 2t+1 next votes for NULL_BLOCK_HASH";
    return true;
  }

  if (pbft_chain_->findPbftBlockInChain(previous_round_next_voted_value_)) {
    LOG(log_nf_) << "In round " << round << " step " << step_
                 << ", find voted value in PBFT chain already. Give up voted value "
                 << previous_round_next_voted_value_;
    return true;
  }

  auto pbft_block = getUnfinalizedBlock_(previous_round_next_voted_value_);
  if (pbft_block) {
    // Have a block, but is it valid?
    if (!pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      // Received the block, but not valid
      return true;
    }
  } else {
    LOG(log_dg_) << "Cannot find PBFT block " << previous_round_next_voted_value_
                 << " in both queue and DB, have not got yet";
  }

  return false;
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

void PbftManager::countVotes_() {
  auto round = getPbftRound();
  while (!monitor_stop_) {
    auto verified_votes = vote_mgr_->copyVerifiedVotes();
    auto unverified_votes = vote_mgr_->copyUnverifiedVotes();
    std::vector<std::shared_ptr<Vote>> votes;
    votes.reserve(verified_votes.size() + unverified_votes.size());
    votes.insert(votes.end(), std::make_move_iterator(verified_votes.begin()),
                 std::make_move_iterator(verified_votes.end()));
    votes.insert(votes.end(), std::make_move_iterator(unverified_votes.begin()),
                 std::make_move_iterator(unverified_votes.end()));

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

std::optional<SyncBlock> PbftManager::processSyncBlock() {
  auto sync_block = sync_queue_.pop();
  const auto &pbft_block = sync_block.first.getPbftBlock();
  const auto &pbft_block_hash = pbft_block->getBlockHash();
  LOG(log_nf_) << "Pop pbft block " << pbft_block_hash << " from synced queue";

  if (pbft_chain_->findPbftBlockInChain(pbft_block_hash)) {
    LOG(log_dg_) << "PBFT block " << pbft_block_hash << " already present in chain.";
    return std::nullopt;
  }

  auto net = network_.lock();
  assert(net);  // Should never happen

  // Check previous hash matches
  if (pbft_block->getPrevBlockHash() != pbft_chain_->getLastPbftBlockHash()) {
    // TODO: should be clear it is related to syncing, was log_er_pbft_sync_
    LOG(log_er_) << "Invalid PBFT block " << pbft_block_hash << "; prevHash: " << pbft_block->getPrevBlockHash()
                 << " from peer " << sync_block.second.abridged() << " received, stop syncing.";
    sync_queue_.clear();
    // Handle malicious peer on network level
    net->handleMaliciousSyncPeer(sync_block.second);
    return std::nullopt;
  }

  // Check cert vote matches
  for (auto const &vote : sync_block.first.getCertVotes()) {
    if (vote->getBlockHash() != pbft_block_hash) {
      LOG(log_er_) << "Invalid cert votes block hash " << vote->getBlockHash() << " instead of " << pbft_block_hash
                   << " from peer " << sync_block.second.abridged() << " received, stop syncing.";
      sync_queue_.clear();
      net->handleMaliciousSyncPeer(sync_block.second);
      return std::nullopt;
    }
  }

  auto order_hash = calculateOrderHash(sync_block.first.getDagBlocks(), sync_block.first.getTransactions());
  if (order_hash != pbft_block->getOrderHash()) {
    LOG(log_er_) << "Order hash incorrect in sync block " << pbft_block_hash << " expected: " << order_hash
                 << " received " << pbft_block->getOrderHash()
                 << "; Dag order: " << sync_block.first.getDagBlocksHashes()
                 << "; Trx order: " << sync_block.first.getTransactionsHashes() << "; from "
                 << sync_block.second.abridged() << ", stop syncing.";
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(sync_block.second);
    return std::nullopt;
  }

  // Check cert votes validation
  try {
    sync_block.first.hasEnoughValidCertVotes(getDposTotalVotesCount(), getSortitionThreshold(), getTwoTPlusOne(),
                                             [this](auto const &addr) { return dposEligibleVoteCount_(addr); });
  } catch (const std::logic_error &e) {
    // Failed cert votes validation, flush synced PBFT queue and set since
    // next block validation depends on the current one
    LOG(log_er_) << e.what();  // log exception text from hasEnoughValidCertVotes
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash
                 << " doesn't have enough valid cert votes. Clear synced PBFT blocks! DPOS total votes count: "
                 << getDposTotalVotesCount();
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(sync_block.second);
    return std::nullopt;
  }

  return std::optional<SyncBlock>(std::move(sync_block.first));
}

void PbftManager::syncBlockQueuePush(SyncBlock &&block, dev::p2p::NodeID const &node_id) {
  const auto period = block.getPbftBlock()->getPeriod();
  if (!sync_queue_.push(std::move(block), node_id, pbft_chain_->getPbftChainSize())) {
    LOG(log_er_) << "Trying to push block with " << period << " period, but current period is "
                 << sync_queue_.getPeriod();
  }
}

size_t PbftManager::syncBlockQueueSize() const { return sync_queue_.size(); }

}  // namespace taraxa
