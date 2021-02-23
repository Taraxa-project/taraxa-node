/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "pbft_manager.hpp"

#include <libdevcore/SHA3.h>

#include <chrono>
#include <string>

#include "chain/final_chain.hpp"
#include "dag/dag.hpp"

namespace taraxa {
using vrf_output_t = vrf_wrapper::vrf_output_t;

PbftManager::PbftManager(PbftConfig const &conf, std::string const &genesis, addr_t node_addr,
                         std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                         std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<FinalChain> final_chain,
                         std::shared_ptr<Executor> executor, secret_t node_sk, vrf_sk_t vrf_sk)
    : LAMBDA_ms_MIN(conf.lambda_ms_min),
      COMMITTEE_SIZE(conf.committee_size),
      DAG_BLOCKS_SIZE(conf.dag_blocks_size),
      GHOST_PATH_MOVE_BACK(conf.ghost_path_move_back),
      RUN_COUNT_VOTES(conf.run_count_votes),
      dag_genesis_(genesis),
      node_addr_(node_addr),
      db_(db),
      pbft_chain_(pbft_chain),
      vote_mgr_(vote_mgr),
      dag_mgr_(dag_mgr),
      dag_blk_mgr_(dag_blk_mgr),
      final_chain_(final_chain),
      executor_(executor),
      node_sk_(node_sk),
      vrf_sk_(vrf_sk) {
  LOG_OBJECTS_CREATE("PBFT_MGR");
  update_dpos_state_();
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setNetwork(std::shared_ptr<Network> network) {
  network_ = network;
  capability_ = network ? network->getTaraxaCapability() : nullptr;
}

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  std::vector<std::string> ghost;
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

  // Initialize PBFT status
  initialState_();

  while (!stopped_) {
    if (stateOperations_()) {
      continue;
    }
    // PBFT states
    switch (state_) {
      case value_proposal_state:
        proposeBlock_();
        setNextState_();
        break;
      case filter_state:
        identifyBlock_();
        setNextState_();
        break;
      case certify_state:
        certifyBlock_();
        setNextState_();
        break;
      case finish_state:
        firstFinish_();
        setNextState_();
        break;
      case finish_polling_state:
        secondFinish_();
        setNextState_();
        if (continue_finish_polling_state_) {
          continue;
        }
        break;
      default:
        LOG(log_er_) << "Unknown PBFT state " << state_;
        assert(false);
    }
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
    res.second = *value;
  }
  return res;
}

uint64_t PbftManager::getPbftRound() const {
  sharedLock_ lock(round_access_);
  return round_;
}

void PbftManager::setPbftRound(uint64_t const round) {
  uniqueLock_ lock(round_access_);
  db_->savePbftMgrField(PbftMgrRoundStep::PbftRound, round);
  round_ = round;
}

size_t PbftManager::getSortitionThreshold() const { return sortition_threshold_; }

size_t PbftManager::getTwoTPlusOne() const { return TWO_T_PLUS_ONE; }

void PbftManager::setTwoTPlusOne(size_t const two_t_plus_one) { TWO_T_PLUS_ONE = two_t_plus_one; }

// Notice: Test purpose
void PbftManager::setSortitionThreshold(size_t const sortition_threshold) {
  sortition_threshold_ = sortition_threshold;
}

void PbftManager::update_dpos_state_() {
  dpos_period_ = pbft_chain_->getPbftChainSize();
  do {
    try {
      eligible_voter_count_ = final_chain_->dpos_eligible_count(dpos_period_);
      break;
    } catch (state_api::ErrFutureBlock &c) {
      LOG(log_nf_) << c.what() << ". PBFT period " << dpos_period_ << " is too far ahead of DPOS, need wait!"
                   << " PBFT chain size " << pbft_chain_->getPbftChainSize() << ", have executed chain size "
                   << final_chain_->last_block_number();
      // Sleep one PBFT lambda time
      thisThreadSleepForMilliSeconds(LAMBDA_ms);
    }
  } while (!stopped_);
}

uint64_t PbftManager::getEligibleVoterCount() const { return eligible_voter_count_; }

bool PbftManager::is_eligible_(addr_t const &addr) { return final_chain_->dpos_is_eligible(dpos_period_, addr); }

bool PbftManager::shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step) {
  //  if (capability_->syncing_) {
  //    LOG(log_tr_) << "PBFT chain is syncing, cannot propose and vote";
  //    return false;
  //  }
  if (!is_eligible_(node_addr_)) {
    LOG(log_tr_) << "Account " << node_addr_ << " is not eligible to vote";
    return false;
  }
  // compute sortition
  VrfPbftMsg msg(pbft_chain_last_block_hash_, type, round, step);
  VrfPbftSortition vrf_sortition(vrf_sk_, msg);
  if (!vrf_sortition.canSpeak(sortition_threshold_, getEligibleVoterCount())) {
    LOG(log_tr_) << "Don't get sortition";
    return false;
  }
  return true;
}

void PbftManager::setPbftStep(size_t const pbft_step) {
  last_step_ = step_;
  db_->savePbftMgrField(PbftMgrRoundStep::PbftStep, pbft_step);
  step_ = pbft_step;

  // if (step_ > MAX_STEPS) {
  //   // Note: We calculate the lambda for a step independently of prior steps
  //   //       in case missed earlier steps.
  //   // LAMBDA_ms = 100 * LAMBDA_ms_MIN;
  //   // LOG(log_nf_) << "Surpassed max steps, relaxing lambda to " << LAMBDA_ms
  //   //             << " ms in round " << getPbftRound() << ", step " << step_;
  //   LAMBDA_ms = LAMBDA_ms_MIN;
  // } else {
  //   LAMBDA_ms = LAMBDA_ms_MIN;
  // }
}

void PbftManager::getNextVotesForLastRound(std::vector<Vote> &next_votes_bundle) {
  sharedLock_ lock(next_votes_access_);
  for (auto &next_vote_for_last_round : next_votes_for_last_round_) {
    next_votes_bundle.emplace_back(next_vote_for_last_round.second);
  }
}

void PbftManager::updateNextVotesForRound(std::vector<Vote> next_votes) {
  LOG(log_nf_) << "Store " << next_votes.size() << " next votes at round " << getPbftRound() << " step " << step_;
  uniqueLock_ lock(next_votes_access_);
  // Cleanup next votes
  next_votes_for_last_round_.clear();
  // Store enough next votes for round and step
  for (auto const &v : next_votes) {
    next_votes_for_last_round_[v.getHash()] = v;
    LOG(log_nf_) << "Store next vote: " << v;
  }
}

void PbftManager::resetStep_() { setPbftStep(1); }

bool PbftManager::resetRound_() {
  bool restart = false;
  // Check if we are synced to the right step ...
  uint64_t consensus_pbft_round = roundDeterminedFromVotes_();
  // Check should be always true...
  auto round = getPbftRound();
  assert(consensus_pbft_round >= round);

  if (consensus_pbft_round > round) {
    LOG(log_nf_) << "From votes determined round " << consensus_pbft_round;
    round_clock_initial_datetime_ = now_;
    uint64_t local_round = round;
    setPbftRound(consensus_pbft_round);
    resetStep_();
    state_ = value_proposal_state;
    LOG(log_dg_) << "Advancing clock to pbft round " << round << ", step 1, and resetting clock.";

    // Identify what block was next voted if any in this last round...
    auto next_voted_block_from_previous_round = nextVotedBlockForRoundAndStep_(votes_, local_round);

    // Update in DB first
    auto batch = db_->createWriteBatch();
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::executed_in_round, false, batch);
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::cert_voted_in_round, false, batch);
    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::own_starting_value_in_round, NULL_BLOCK_HASH, batch);
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_null_block_hash, false, batch);
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_soft_value, false, batch);
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::soft_voted_block_in_round, false, batch);
    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::soft_voted_block_hash_in_round, NULL_BLOCK_HASH, batch);
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_block_in_previous_round,
                                 next_voted_block_from_previous_round.second, batch);
    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::next_voted_block_hash_in_previous_round,
                                     next_voted_block_from_previous_round.first, batch);
    db_->commitWriteBatch(batch);

    have_executed_this_round_ = false;
    should_have_cert_voted_in_this_round_ = false;
    // reset starting value to NULL_BLOCK_HASH
    own_starting_value_for_round_ = NULL_BLOCK_HASH;
    // reset next voted value since start a new round
    next_voted_null_block_hash_ = false;
    next_voted_soft_value_ = false;

    // Key thing is to set .second to false to mark that we have not
    // identified a soft voted block in the new upcoming round...
    soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, false);

    // Identify what block was next voted if any in this last round...
    next_voted_block_from_previous_round_ = next_voted_block_from_previous_round;

    if (executed_pbft_block_) {
      update_dpos_state_();
      // reset sortition_threshold and TWO_T_PLUS_ONE
      updateTwoTPlusOneAndThreshold_();
      db_->savePbftMgrStatus(PbftMgrStatus::executed_block, false);
      executed_pbft_block_ = false;
    }

    LAMBDA_ms = LAMBDA_ms_MIN;
    last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
    current_step_clock_initial_datetime_ = std::chrono::system_clock::now();

    // p2p connection syncing should cover this situation, sync here for safe
    if (consensus_pbft_round > local_round + 1 && capability_->syncing_ == false) {
      LOG(log_nf_) << "Quorum determined round " << consensus_pbft_round << " > 1 + current round " << local_round
                   << " local round, need to broadcast request for missing certified blocks";
      // NOTE: Advance round and step before calling sync to make sure
      //       sync call won't be supressed for being too
      //       recent (ie. same round and step)
      syncPbftChainFromPeers_(false);
      auto batch = db_->createWriteBatch();
      db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_block_in_previous_round, false, batch);
      db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::next_voted_block_hash_in_previous_round, NULL_BLOCK_HASH,
                                       batch);
      db_->commitWriteBatch(batch);
      next_voted_block_from_previous_round_ = std::make_pair(NULL_BLOCK_HASH, false);
    }
    // Restart while loop...
    restart = true;
  }
  return restart;
}

void PbftManager::sleep_() {
  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
  auto time_to_sleep_for_ms = next_step_time_ms_ - elapsed_time_in_round_ms_;
  if (time_to_sleep_for_ms > 0) {
    LOG(log_tr_) << "Time to sleep(ms): " << time_to_sleep_for_ms << " in round " << getPbftRound() << ", step "
                 << step_;
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(time_to_sleep_for_ms));
  }
}

void PbftManager::initialState_() {
  // Initial PBFT state
  LAMBDA_ms = LAMBDA_ms_MIN;
  STEP_4_DELAY = 2 * LAMBDA_ms;

  auto round = db_->getPbftMgrField(PbftMgrRoundStep::PbftRound);
  auto step = db_->getPbftMgrField(PbftMgrRoundStep::PbftStep);
  if (round == 1 && step == 1) {
    // Start at initialization
    state_ = value_proposal_state;
    setPbftStep(1);
  } else {
    // Start from DB
    state_ = finish_state;
    setPbftStep(4);
  }
  setPbftRound(round);

  // Initial last sync request
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  auto own_starting_value = db_->getPbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round);
  if (own_starting_value) {
    // From DB
    own_starting_value_for_round_ = *own_starting_value;
  } else {
    // Default value
    own_starting_value_for_round_ = NULL_BLOCK_HASH;
  }

  auto soft_voted_block_hash = db_->getPbftMgrVotedValue(PbftMgrVotedValue::soft_voted_block_hash_in_round);
  auto soft_voted_block = db_->getPbftMgrStatus(PbftMgrStatus::soft_voted_block_in_round);
  if (soft_voted_block_hash) {
    // From DB
    soft_voted_block_for_this_round_ = std::make_pair(*soft_voted_block_hash, soft_voted_block);
  } else {
    // Default value
    soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, soft_voted_block);
  }

  auto next_voted_block_hash_in_previous_round =
      db_->getPbftMgrVotedValue(PbftMgrVotedValue::next_voted_block_hash_in_previous_round);
  auto next_voted_block_in_previous_round = db_->getPbftMgrStatus(PbftMgrStatus::next_voted_block_in_previous_round);
  if (next_voted_block_hash_in_previous_round) {
    // From DB
    next_voted_block_from_previous_round_ =
        std::make_pair(*next_voted_block_hash_in_previous_round, next_voted_block_in_previous_round);
  } else {
    // Default value
    next_voted_block_from_previous_round_ = std::make_pair(NULL_BLOCK_HASH, next_voted_block_in_previous_round);
  }

  executed_pbft_block_ = db_->getPbftMgrStatus(PbftMgrStatus::executed_block);
  have_executed_this_round_ = db_->getPbftMgrStatus(PbftMgrStatus::executed_in_round);
  should_have_cert_voted_in_this_round_ = db_->getPbftMgrStatus(PbftMgrStatus::cert_voted_in_round);
  next_voted_soft_value_ = db_->getPbftMgrStatus(PbftMgrStatus::next_voted_soft_value);
  next_voted_null_block_hash_ = db_->getPbftMgrStatus(PbftMgrStatus::next_voted_null_block_hash);

  round_clock_initial_datetime_ = std::chrono::system_clock::now();
  current_step_clock_initial_datetime_ = round_clock_initial_datetime_;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  next_step_time_ms_ = 0;

  // Initialize TWO_T_PLUS_ONE and sortition_threshold
  updateTwoTPlusOneAndThreshold_();

  // Initialize last block hash (PBFT genesis block in beginning)
  pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();
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
      if (continue_finish_polling_state_) {
        continueFinishPollingState_(step_ + 2);
      } else {
        if (loop_back_finish_state_) {
          loopBackFinishState_();
        } else {
          next_step_time_ms_ += POLLING_INTERVAL_ms;
        }
      }
      break;
    default:
      LOG(log_er_) << "Unknown PBFT state " << state_;
      assert(false);
  }
  if (!continue_finish_polling_state_) {
    LOG(log_tr_) << "next step time(ms): " << next_step_time_ms_;
  }
}

void PbftManager::setFilterState_() {
  state_ = filter_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * LAMBDA_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::setCertifyState_() {
  state_ = certify_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * LAMBDA_ms;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::setFinishState_() {
  LOG(log_dg_) << "Will go to first finish State";
  state_ = finish_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 4 * LAMBDA_ms + STEP_4_DELAY;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::setFinishPollingState_() {
  state_ = finish_polling_state;
  setPbftStep(step_ + 1);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_soft_value, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_null_block_hash, false, batch);
  db_->commitWriteBatch(batch);
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::continueFinishPollingState_(size_t step) {
  state_ = finish_polling_state;
  setPbftStep(step);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_soft_value, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_null_block_hash, false, batch);
  db_->commitWriteBatch(batch);
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
}

void PbftManager::loopBackFinishState_() {
  auto round = getPbftRound();
  LOG(log_dg_) << "CONSENSUSDBG round " << round << " , step " << step_
               << " | next_voted_soft_value_ = " << next_voted_soft_value_
               << " soft block = " << soft_voted_block_for_this_round_.first
               << " next_voted_null_block_hash_ = " << next_voted_null_block_hash_
               << " next_voted_block_from_previous_round_ = " << next_voted_block_from_previous_round_.first
               << " cert voted = " << (cert_voted_values_for_round_.find(round) != cert_voted_values_for_round_.end());
  state_ = finish_state;
  setPbftStep(step_ + 1);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_soft_value, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_null_block_hash, false, batch);
  db_->commitWriteBatch(batch);
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
  next_step_time_ms_ = step_ * LAMBDA_ms + STEP_4_DELAY;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

bool PbftManager::stateOperations_() {
  // Reset continue finish polling state
  continue_finish_polling_state_ = false;

  // NOTE: PUSHING OF SYNCED BLOCKS CAN TAKE A LONG TIME
  //       SHOULD DO BEFORE WE SET THE ELAPSED TIME IN ROUND
  // push synced pbft blocks into chain
  pushSyncedPbftBlocksIntoChain_();

  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();

  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT current round is " << round;
  LOG(log_tr_) << "PBFT current step is " << step_;

  // Get votes
  votes_ = vote_mgr_->getVotes(round, pbft_chain_last_block_hash_, sortition_threshold_, getEligibleVoterCount(),
                               [this](auto const &addr) { return is_eligible_(addr); });
  LOG(log_tr_) << "There are " << votes_.size() << " total votes in round " << round;

  // CHECK IF WE HAVE RECEIVED 2t+1 CERT VOTES FOR A BLOCK IN OUR CURRENT
  // ROUND.  IF WE HAVE THEN WE EXECUTE THE BLOCK
  // ONLY CHECK IF HAVE *NOT* YET EXECUTED THIS ROUND...
  if (state_ == certify_state && !have_executed_this_round_) {
    std::vector<Vote> cert_votes_for_round = getVotesOfTypeFromVotesForRoundAndStep_(
        cert_vote_type, votes_, round, 3, std::make_pair(NULL_BLOCK_HASH, false));
    std::pair<blk_hash_t, bool> cert_voted_block_hash = blockWithEnoughVotes_(cert_votes_for_round);
    if (cert_voted_block_hash.second) {
      LOG(log_dg_) << "PBFT block " << cert_voted_block_hash.first << " has enough certed votes";
      // put pbft block into chain
      if (pushCertVotedPbftBlockIntoChain_(cert_voted_block_hash.first, cert_votes_for_round)) {
        push_block_values_for_round_[round] = cert_voted_block_hash.first;
        db_->savePbftMgrStatus(PbftMgrStatus::executed_in_round, true);
        have_executed_this_round_ = true;
        LOG(log_nf_) << "Write " << cert_votes_for_round.size() << " votes ... in round " << round;
        duration_ = std::chrono::system_clock::now() - now_;
        auto execute_trxs_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
        LOG(log_dg_) << "Pushing PBFT block and Execution spent " << execute_trxs_in_ms << " ms. in round " << round;
        // Restart while loop
        return true;
      }
    }
  }
  // We skip step 4 due to having missed it while executing....
  if (state_ == certify_state && have_executed_this_round_ &&
      elapsed_time_in_round_ms_ > 4 * LAMBDA_ms + STEP_4_DELAY + 2 * POLLING_INTERVAL_ms) {
    LOG(log_dg_) << "Skipping step 4 due to execution, will go to step 5 in round " << round;
    setPbftStep(5);
    state_ = finish_polling_state;
  }

  return resetRound_();
}

void PbftManager::proposeBlock_() {
  // Value Proposal
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT value proposal state in round " << round;
  if (next_voted_block_from_previous_round_.second) {
    LOG(log_nf_) << "We have a next voted block from previous round " << round - 1;
    if (next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH) {
      LOG(log_nf_) << "Previous round next voted block is NULL_BLOCK_HASH";
    } else {
      LOG(log_nf_) << "Previous round next voted block is NOT NULL_BLOCK_HASH";
    }
  } else {
    LOG(log_nf_) << "No next voted block from previous round " << round - 1;
  }

  if (round == 1) {
    if (shouldSpeak(propose_vote_type, round, step_)) {
      LOG(log_nf_) << "Proposing value of NULL_BLOCK_HASH " << NULL_BLOCK_HASH << " for round 1 by protocol";
      placeVote_(own_starting_value_for_round_, propose_vote_type, round, step_);
    }
  } else if (push_block_values_for_round_.count(round - 1) ||
             (round >= 2 && next_voted_block_from_previous_round_.second &&
              next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH)) {
    if (shouldSpeak(propose_vote_type, round, step_)) {
      // PBFT block only be proposed once in one period
      if (!proposed_block_hash_.second || proposed_block_hash_.first == NULL_BLOCK_HASH) {
        // Propose value...
        proposed_block_hash_ = proposeMyPbftBlock_();
      }
      if (proposed_block_hash_.second) {
        db_->savePbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round, proposed_block_hash_.first);
        own_starting_value_for_round_ = proposed_block_hash_.first;
        LOG(log_nf_) << "Proposing own starting value " << own_starting_value_for_round_ << " for round " << round;
        placeVote_(proposed_block_hash_.first, propose_vote_type, round, step_);
      }
    }
  } else if (round >= 2) {
    if (next_voted_block_from_previous_round_.second &&
        next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
      db_->savePbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round,
                                 next_voted_block_from_previous_round_.first);
      own_starting_value_for_round_ = next_voted_block_from_previous_round_.first;
      if (shouldSpeak(propose_vote_type, round, step_)) {
        LOG(log_nf_) << "Proposing next voted block " << next_voted_block_from_previous_round_.first
                     << " from previous round, for round " << round;
        placeVote_(next_voted_block_from_previous_round_.first, propose_vote_type, round, step_);
      }
    }
  }
}

void PbftManager::identifyBlock_() {
  // The Filtering Step
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT filtering state in round " << round;
  if (round == 1 || push_block_values_for_round_.count(round - 1) ||
      (round >= 2 && next_voted_block_from_previous_round_.second &&
       next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH)) {
    // Identity leader
    std::pair<blk_hash_t, bool> leader_block = identifyLeaderBlock_(votes_);
    if (leader_block.second) {
      db_->savePbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round, leader_block.first);
      own_starting_value_for_round_ = leader_block.first;
      LOG(log_dg_) << "Identify leader block " << leader_block.first << " at round " << round;
      if (shouldSpeak(soft_vote_type, round, step_)) {
        LOG(log_dg_) << "Soft voting block " << leader_block.first << " at round " << round;
        placeVote_(leader_block.first, soft_vote_type, round, step_);
      }
    }
  } else if (round >= 2 && next_voted_block_from_previous_round_.second &&
             next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
    if (shouldSpeak(soft_vote_type, round, step_)) {
      LOG(log_dg_) << "Soft voting " << next_voted_block_from_previous_round_.first << " from previous round";
      placeVote_(next_voted_block_from_previous_round_.first, soft_vote_type, round, step_);
    }
  }
}

void PbftManager::certifyBlock_() {
  // The Certifying Step
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT certifying state in round " << round;
  if (elapsed_time_in_round_ms_ < 2 * LAMBDA_ms) {
    // Should not happen, add log here for safety checking
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only " << elapsed_time_in_round_ms_ << " (ms) in round "
                 << round;
  }

  go_finish_state_ = elapsed_time_in_round_ms_ > 4 * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms;
  if (go_finish_state_) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in round " << round;
  } else if (!should_have_cert_voted_in_this_round_) {
    LOG(log_tr_) << "In step 3";
    if (!soft_voted_block_for_this_round_.second) {
      auto soft_voted_block = softVotedBlockForRound_(votes_, round);
      auto batch = db_->createWriteBatch();
      db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::soft_voted_block_hash_in_round, soft_voted_block.first,
                                       batch);
      db_->addPbftMgrStatusToBatch(PbftMgrStatus::soft_voted_block_in_round, soft_voted_block.second, batch);
      db_->commitWriteBatch(batch);
      soft_voted_block_for_this_round_ = soft_voted_block;
    }
    if (soft_voted_block_for_this_round_.second && soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH &&
        comparePbftBlockScheduleWithDAGblocks_(soft_voted_block_for_this_round_.first)) {
      LOG(log_tr_) << "Finished comparePbftBlockScheduleWithDAGblocks_";

      // NOTE: If we have already executed this round
      //       then block won't be found in unverified queue...
      bool executed_soft_voted_block_for_this_round = false;
      if (have_executed_this_round_) {
        LOG(log_tr_) << "Have already executed before certifying in step 3 in round " << round;
        if (pbft_chain_last_block_hash_ == soft_voted_block_for_this_round_.first) {
          LOG(log_tr_) << "Having executed, last block in chain is the "
                          "soft voted block in round "
                       << round;
          executed_soft_voted_block_for_this_round = true;
        }
      }

      bool unverified_soft_vote_block_for_this_round_is_valid = false;
      if (!executed_soft_voted_block_for_this_round) {
        if (checkPbftBlockValid_(soft_voted_block_for_this_round_.first)) {
          LOG(log_tr_) << "checkPbftBlockValid_ returned true";
          unverified_soft_vote_block_for_this_round_is_valid = true;
        } else {
          // Get partition, need send request to get missing pbft blocks from peers
          LOG(log_er_)
              << "Soft voted block for this round appears to be invalid, we must be out of sync with pbft chain";
          if (!capability_->syncing_) {
            syncPbftChainFromPeers_(false);
          }
        }
      }

      if (executed_soft_voted_block_for_this_round || unverified_soft_vote_block_for_this_round_is_valid) {
        cert_voted_values_for_round_[round] = soft_voted_block_for_this_round_.first;

        // NEED TO KEEP POLLING TO SEE IF WE HAVE 2t+1 cert votes...
        // Here we would cert vote if we can speak....
        db_->savePbftMgrStatus(PbftMgrStatus::cert_voted_in_round, true);
        should_have_cert_voted_in_this_round_ = true;
        if (shouldSpeak(cert_vote_type, round, step_)) {
          LOG(log_dg_) << "Cert voting " << soft_voted_block_for_this_round_.first << " for round " << round;
          // generate cert vote
          placeVote_(soft_voted_block_for_this_round_.first, cert_vote_type, round, step_);
        }
      }
    }
  }
}

void PbftManager::firstFinish_() {
  // Even number steps from 4 are in first finish
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT first finishing state at step " << step_ << " in round " << round;
  if (shouldSpeak(next_vote_type, round, step_)) {
    if (cert_voted_values_for_round_.find(round) != cert_voted_values_for_round_.end()) {
      LOG(log_dg_) << "Next voting cert voted value " << cert_voted_values_for_round_[round] << " for round " << round
                   << " , step " << step_;
      placeVote_(cert_voted_values_for_round_[round], next_vote_type, round, step_);
    } else if (round >= 2 && next_voted_block_from_previous_round_.second &&
               next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH) {
      LOG(log_dg_) << "Next voting NULL BLOCK for round " << round << ", at step " << step_;
      placeVote_(NULL_BLOCK_HASH, next_vote_type, round, step_);
    } else {
      LOG(log_dg_) << "Next voting nodes own starting value " << own_starting_value_for_round_ << " for round " << round
                   << ", at step " << step_;
      placeVote_(own_starting_value_for_round_, next_vote_type, round, step_);
    }
  }
}

void PbftManager::secondFinish_() {
  // Odd number steps from 5 are in second finish
  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT second finishing state at step " << step_ << " in round " << round;
  long end_time_for_step = (step_ + 1) * LAMBDA_ms + STEP_4_DELAY + 2 * POLLING_INTERVAL_ms;
  // if (step_ > MAX_STEPS) {
  //  u_long LAMBDA_ms_BIG = 100 * LAMBDA_ms_MIN;
  //  end_time_for_step = MAX_STEPS * LAMBDA_ms_MIN +
  //                      (step_ - MAX_STEPS + 1) * LAMBDA_ms_BIG + STEP_4_DELAY
  //                      + 2 * POLLING_INTERVAL_ms;
  //}*/
  if (elapsed_time_in_round_ms_ > end_time_for_step) {
    // Should not happen, add log here for safety checking
    if (have_executed_this_round_) {
      LOG(log_dg_) << "PBFT Reached round " << round << " at step " << step_ << " late due to execution";
    } else {
      LOG(log_dg_) << "PBFT Reached round " << round << " at step " << step_ << " late without executing";
    }
    continue_finish_polling_state_ = true;
    return;
  }

  if (shouldSpeak(next_vote_type, round, step_)) {
    if (!soft_voted_block_for_this_round_.second) {
      auto soft_voted_block = softVotedBlockForRound_(votes_, round);
      auto batch = db_->createWriteBatch();
      db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::soft_voted_block_hash_in_round, soft_voted_block.first,
                                       batch);
      db_->addPbftMgrStatusToBatch(PbftMgrStatus::soft_voted_block_in_round, soft_voted_block.second, batch);
      db_->commitWriteBatch(batch);
      soft_voted_block_for_this_round_ = soft_voted_block;
    }
    if (!next_voted_soft_value_ && soft_voted_block_for_this_round_.second &&
        soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH) {
      LOG(log_dg_) << "Next voting " << soft_voted_block_for_this_round_.first << " for round " << round << ", at step "
                   << step_;
      placeVote_(soft_voted_block_for_this_round_.first, next_vote_type, round, step_);
      db_->savePbftMgrStatus(PbftMgrStatus::next_voted_soft_value, true);
      next_voted_soft_value_ = true;
    }
    if (!next_voted_null_block_hash_ && round >= 2 &&
        ((next_voted_block_from_previous_round_.second &&
          next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH) ||
         push_block_values_for_round_.count(round - 1)) &&
        (cert_voted_values_for_round_.find(round) == cert_voted_values_for_round_.end())) {
      LOG(log_dg_) << "Next voting NULL BLOCK for round " << round << ", at step " << step_;
      placeVote_(NULL_BLOCK_HASH, next_vote_type, round, step_);
      db_->savePbftMgrStatus(PbftMgrStatus::next_voted_null_block_hash, true);
      next_voted_null_block_hash_ = true;
    }
  }

  if (step_ > MAX_STEPS && !capability_->syncing_ && !syncRequestedAlreadyThisStep_()) {
    LOG(log_wr_) << "Suspect PBFT manger stucked, inaccurate 2t+1, need to broadcast request for missing blocks";
    syncPbftChainFromPeers_(true);
  }

  if (step_ > MAX_STEPS) {
    LOG(log_tr_) << "Suspect PBFT round behind, need to sync with peers";
    syncNextVotes_();
  }

  loop_back_finish_state_ = elapsed_time_in_round_ms_ > (step_ + 1) * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms;
}

// There is a quorum of next-votes and set determine that round p should be the current round...
uint64_t PbftManager::roundDeterminedFromVotes_() {
  // <<vote_round, vote_step>, count>, <round, step> store in reverse order
  std::map<std::pair<uint64_t, size_t>, size_t, std::greater<std::pair<uint64_t, size_t>>>
      next_votes_tally_by_round_step;
  auto round = getPbftRound();

  for (Vote &v : votes_) {
    if (v.getType() != next_vote_type) {
      continue;
    }
    std::pair<uint64_t, size_t> round_step = std::make_pair(v.getRound(), v.getStep());
    if (round_step.first >= round) {
      if (next_votes_tally_by_round_step.find(round_step) != next_votes_tally_by_round_step.end()) {
        next_votes_tally_by_round_step[round_step] += 1;
      } else {
        next_votes_tally_by_round_step[round_step] = 1;
      }
    }
  }

  for (auto &rs_votes : next_votes_tally_by_round_step) {
    if (rs_votes.second >= TWO_T_PLUS_ONE) {
      std::vector<Vote> next_votes_for_round_step = getVotesOfTypeFromVotesForRoundAndStep_(
          next_vote_type, votes_, rs_votes.first.first, rs_votes.first.second, std::make_pair(NULL_BLOCK_HASH, false));
      if (blockWithEnoughVotes_(next_votes_for_round_step).second) {
        LOG(log_dg_) << "Found sufficient next votes in round " << rs_votes.first.first << ", step "
                     << rs_votes.first.second;
        updateNextVotesForRound(next_votes_for_round_step);
        return rs_votes.first.first + 1;
      }
    }
  }
  return round;
}

// Assumption is that all votes are in the same round and of same type...
std::pair<blk_hash_t, bool> PbftManager::blockWithEnoughVotes_(std::vector<Vote> const &votes) const {
  bool is_first_block = true;
  PbftVoteTypes vote_type;
  uint64_t vote_round;
  blk_hash_t blockhash;
  // <block_hash, count>, store in reverse order
  std::map<blk_hash_t, size_t, std::greater<blk_hash_t>> tally_by_blockhash;

  for (Vote const &v : votes) {
    if (is_first_block) {
      vote_type = v.getType();
      vote_round = v.getRound();
      is_first_block = false;
    } else {
      bool vote_type_and_round_is_consistent = (vote_type == v.getType() && vote_round == v.getRound());
      if (!vote_type_and_round_is_consistent) {
        LOG(log_er_) << "Vote types and rounds were not internally"
                        " consistent!";
        assert(false);
        // return std::make_pair(NULL_BLOCK_HASH, false);
      }
    }

    blockhash = v.getBlockHash();
    if (tally_by_blockhash.find(blockhash) != tally_by_blockhash.end()) {
      tally_by_blockhash[blockhash] += 1;
    } else {
      tally_by_blockhash[blockhash] = 1;
    }

    for (auto const &blockhash_pair : tally_by_blockhash) {
      if (blockhash_pair.second >= TWO_T_PLUS_ONE) {
        LOG(log_dg_) << "find block hash " << blockhash_pair.first << " vote type " << vote_type << " in round "
                     << vote_round << " has " << blockhash_pair.second << " votes";
        return std::make_pair(blockhash_pair.first, true);
      } else {
        LOG(log_tr_) << "Don't have enough votes. block hash " << blockhash_pair.first << " vote type " << vote_type
                     << " in round " << vote_round << " has " << blockhash_pair.second << " votes"
                     << " (2TP1 = " << TWO_T_PLUS_ONE << ")";
      }
    }
  }

  return std::make_pair(NULL_BLOCK_HASH, false);
}

std::map<size_t, std::vector<Vote>, std::greater<size_t>> PbftManager::getVotesOfTypeFromVotesForRoundByStep_(
    PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round, std::pair<blk_hash_t, bool> blockhash) {
  // <vote_step, votes> reverse order
  std::map<size_t, std::vector<Vote>, std::greater<size_t>> requested_votes_by_step;

  for (Vote &v : votes) {
    if (v.getType() == vote_type && v.getRound() == round &&
        (blockhash.second == false || blockhash.first == v.getBlockHash())) {
      size_t step = v.getStep();
      if (requested_votes_by_step.find(step) == requested_votes_by_step.end()) {
        requested_votes_by_step[step] = {v};
      } else {
        requested_votes_by_step[step].emplace_back(v);
      }
    }
  }

  return requested_votes_by_step;
}

std::vector<Vote> PbftManager::getVotesOfTypeFromVotesForRoundAndStep_(PbftVoteTypes vote_type,
                                                                       std::vector<Vote> &votes, uint64_t round,
                                                                       size_t step,
                                                                       std::pair<blk_hash_t, bool> blockhash) {
  std::vector<Vote> votes_of_requested_type;
  std::copy_if(votes.begin(), votes.end(), std::back_inserter(votes_of_requested_type),
               [vote_type, round, step, blockhash](Vote const &v) {
                 return (v.getType() == vote_type && v.getRound() == round && v.getStep() == step &&
                         (blockhash.second == false || blockhash.first == v.getBlockHash()));
               });

  return votes_of_requested_type;
}

std::pair<blk_hash_t, bool> PbftManager::nextVotedBlockForRoundAndStep_(std::vector<Vote> &votes, uint64_t round) {
  //<vote_step, votes> reverse order
  std::map<size_t, std::vector<Vote>, std::greater<size_t>> next_votes_in_round_by_step =
      getVotesOfTypeFromVotesForRoundByStep_(next_vote_type, votes, round, std::make_pair(NULL_BLOCK_HASH, false));

  std::pair<blk_hash_t, bool> next_vote_block_hash = std::make_pair(NULL_BLOCK_HASH, false);
  for (auto &sv : next_votes_in_round_by_step) {
    next_vote_block_hash = blockWithEnoughVotes_(sv.second);
    if (next_vote_block_hash.second) {
      return next_vote_block_hash;
    }
  }
  return next_vote_block_hash;
}

Vote PbftManager::generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t round, size_t step,
                               blk_hash_t const &last_pbft_block_hash) {
  // sortition proof
  VrfPbftMsg msg(last_pbft_block_hash, type, round, step);
  VrfPbftSortition vrf_sortition(vrf_sk_, msg);
  Vote vote(node_sk_, vrf_sortition, blockhash);

  LOG(log_dg_) << "last pbft block hash " << last_pbft_block_hash << " vote: " << vote.getHash();
  return vote;
}

void PbftManager::placeVote_(taraxa::blk_hash_t const &blockhash, PbftVoteTypes vote_type, uint64_t round,
                             size_t step) {
  Vote vote = generateVote(blockhash, vote_type, round, step, pbft_chain_last_block_hash_);
  vote_mgr_->addVote(vote);
  LOG(log_dg_) << "vote block hash: " << blockhash << " vote type: " << vote_type << " round: " << round
               << " step: " << step << " vote hash " << vote.getHash();
  // pbft vote broadcast
  network_->onNewPbftVote(vote);
}

std::pair<blk_hash_t, bool> PbftManager::softVotedBlockForRound_(std::vector<taraxa::Vote> &votes, uint64_t round) {
  std::vector<Vote> soft_votes_for_round =
      getVotesOfTypeFromVotesForRoundAndStep_(soft_vote_type, votes, round, 2, std::make_pair(NULL_BLOCK_HASH, false));

  return blockWithEnoughVotes_(soft_votes_for_round);
}

std::pair<blk_hash_t, bool> PbftManager::proposeMyPbftBlock_() {
  LOG(log_dg_) << "Into propose PBFT block";
  std::string last_period_dag_anchor_block_hash;
  if (pbft_chain_last_block_hash_) {
    last_period_dag_anchor_block_hash =
        pbft_chain_->getPbftBlockInChain(pbft_chain_last_block_hash_).getPivotDagBlockHash().toString();
  } else {
    // First PBFT pivot block
    last_period_dag_anchor_block_hash = dag_genesis_;
  }

  std::vector<std::string> ghost;
  dag_mgr_->getGhostPath(last_period_dag_anchor_block_hash, ghost);
  LOG(log_dg_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at lease include the last period dag anchor
  // block
  if (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. No new DAG blocks generated, PBFT "
                    "propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  blk_hash_t dag_block_hash;
  if (ghost.size() <= DAG_BLOCKS_SIZE) {
    // Move back GHOST_PATH_MOVE_BACK DAG blocks for DAG sycning
    int ghost_index = ghost.size() - 1 - GHOST_PATH_MOVE_BACK;
    if (ghost_index <= 0) {
      ghost_index = 0;
    }
    while (ghost_index < ghost.size() - 1) {
      if (ghost[ghost_index] != last_period_dag_anchor_block_hash) {
        break;
      }
      ghost_index += 1;
    }
    dag_block_hash = blk_hash_t(ghost[ghost_index]);
  } else {
    dag_block_hash = blk_hash_t(ghost[DAG_BLOCKS_SIZE - 1]);
  }
  if (dag_block_hash.toString() == dag_genesis_) {
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis " << dag_block_hash
                 << " PBFT propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // compare with last dag block hash. If they are same, which means no new
  // dag blocks generated since last round. In that case PBFT proposer should
  // propose NULL BLOCK HASH as their value and not produce a new block. In
  // practice this should never happen
  if (dag_block_hash.toString() == last_period_dag_anchor_block_hash) {
    LOG(log_dg_) << "Last period DAG anchor block hash " << dag_block_hash
                 << " No new DAG blocks generated, PBFT propose NULL_BLOCK_HASH";
    LOG(log_dg_) << "Ghost: " << ghost;
    return std::make_pair(NULL_BLOCK_HASH, true);
  }

  uint64_t propose_pbft_period = pbft_chain_->getPbftChainSize() + 1;
  addr_t beneficiary = node_addr_;
  // generate generate pbft block
  auto pbft_block =
      s_ptr(new PbftBlock(pbft_chain_last_block_hash_, dag_block_hash, propose_pbft_period, beneficiary, node_sk_));
  // push pbft block
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
  // broadcast pbft block
  network_->onNewPbftBlock(*pbft_block);

  LOG(log_dg_) << node_addr_ << " propose PBFT block succussful! "
               << " in round: " << getPbftRound() << " in step: " << step_ << " PBFT block: " << pbft_block;
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

  for (auto i = 0; i < trx_overlap_table->size(); i++) {
    blk_hash_t &dag_block_hash = (*trx_overlap_table)[i].first;
    auto blk = dag_blk_mgr_->getDagBlock(dag_block_hash);
    if (!blk) {
      LOG(log_er_) << "Cannot create schedule block, DAG block missing " << dag_block_hash;
      continue;
    }

    auto num_trx = blk->getTrxs().size();
    std::vector<uint> block_trx_modes;
    for (auto j = 0; j < num_trx; j++) {
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

std::pair<blk_hash_t, bool> PbftManager::identifyLeaderBlock_(std::vector<Vote> const &votes) {
  auto round = getPbftRound();
  LOG(log_dg_) << "Into identify leader block, in round " << round;
  // each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<vrf_output_t, blk_hash_t>> leader_candidates;
  for (auto const &v : votes) {
    if (v.getRound() == round && v.getType() == propose_vote_type) {
      // We should not pick any null block as leader (proposed when
      // no new blocks found, or maliciously) if others have blocks.
      if (round == 1 || v.getBlockHash() != NULL_BLOCK_HASH) {
        leader_candidates.emplace_back(std::make_pair(v.getCredential(), v.getBlockHash()));
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

bool PbftManager::checkPbftBlockValid_(blk_hash_t const &block_hash) const {
  auto cert_voted_block = pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!cert_voted_block) {
    LOG(log_er_) << "Cannot find the unverified pbft block, block hash " << block_hash;
    return false;
  }
  return pbft_chain_->checkPbftBlockValidation(*cert_voted_block);
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return getPbftRound() == pbft_round_last_requested_sync_ && step_ == pbft_step_last_requested_sync_;
}

void PbftManager::syncPbftChainFromPeers_(bool force) {
  if (stopped_) return;
  if (!pbft_chain_->pbftSyncedQueueEmpty()) {
    LOG(log_dg_) << "DAG has not synced yet. PBFT chain skips syncing";
    return;
  }

  if (!capability_->syncing_ && !syncRequestedAlreadyThisStep_()) {
    auto round = getPbftRound();
    LOG(log_nf_) << "Restarting pbft sync. In round " << round << ", in step " << step_ << ", forced " << force
                 << ", Send request to ask missing blocks";
    capability_->restartSyncingPbft(force);
    pbft_round_last_requested_sync_ = round;
    pbft_step_last_requested_sync_ = step_;
  }
}

bool PbftManager::nextVotesSyncAlreadyThisRoundStep_() {
  return getPbftRound() == pbft_round_last_next_votes_sync_ && step_ == pbft_step_last_next_votes_sync_;
}

void PbftManager::syncNextVotes_() {
  if (stopped_) {
    return;
  }
  //  if (capability_->syncing_) {
  //    LOG(log_dg_) << "Cannot sync next votes bundle, PBFT chain is syncing";
  //    return;
  //  }

  if (!nextVotesSyncAlreadyThisRoundStep_()) {
    auto round = getPbftRound();
    LOG(log_wr_) << "Syncing next votes. Send syncing request at round " << round << ", step " << step_;
    capability_->syncPbftNextVotes(round);
    pbft_round_last_next_votes_sync_ = round;
    pbft_step_last_next_votes_sync_ = step_;
  }
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(blk_hash_t const &pbft_block_hash) {
  auto pbft_block = pbft_chain_->getUnverifiedPbftBlock(pbft_block_hash);
  if (!pbft_block) {
    LOG(log_dg_) << "Have not got the PBFT block yet. block hash: " << pbft_block_hash;
    return false;
  }
  return comparePbftBlockScheduleWithDAGblocks_(*pbft_block);
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(PbftBlock const &pbft_block) {
  auto const &anchor_hash = pbft_block.getPivotDagBlockHash();
  if (!dag_mgr_->getDagBlockOrder(anchor_hash).second->empty()) {
    return true;
  }
  auto last_period = pbft_chain_->getPbftChainSize();
  LOG(log_nf_) << "DAG blocks have not sync yet. In period: " << last_period << ", anchor block hash " << anchor_hash
               << " is not found locally";
  if (state_ == finish_state && !have_executed_this_round_ && !capability_->syncing_ &&
      !syncRequestedAlreadyThisStep_()) {
    LOG(log_nf_) << "DAG blocks have not sync yet. In period: " << last_period << " PBFT block anchor: " << anchor_hash
                 << " .. Triggering sync request";
    syncPbftChainFromPeers_(true);
  }
  return false;
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(taraxa::blk_hash_t const &cert_voted_block_hash,
                                                   std::vector<Vote> const &cert_votes_for_round) {
  if (!checkPbftBlockValid_(cert_voted_block_hash)) {
    // Get partition, need send request to get missing pbft blocks from peers
    LOG(log_er_) << "Cert voted block " << cert_voted_block_hash
                 << " is invalid, we must be out of sync with pbft chain";
    if (capability_->syncing_ == false) {
      syncPbftChainFromPeers_(false);
    }
    return false;
  }
  auto pbft_block = pbft_chain_->getUnverifiedPbftBlock(cert_voted_block_hash);
  if (!pbft_block) {
    LOG(log_er_) << "Can not find the cert vote block hash " << cert_voted_block_hash << " in pbft queue";
    return false;
  }
  if (!comparePbftBlockScheduleWithDAGblocks_(*pbft_block)) {
    return false;
  }
  PbftBlockCert pbft_block_cert_votes(*pbft_block, cert_votes_for_round);
  if (!pushPbftBlock_(pbft_block_cert_votes)) {
    LOG(log_er_) << "Failed push PBFT block " << pbft_block->getBlockHash() << " into chain";
    return false;
  }
  // cleanup PBFT unverified blocks table
  pbft_chain_->cleanupUnverifiedPbftBlocks(*pbft_block);
  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain_() {
  size_t pbft_synced_queue_size;
  while (!pbft_chain_->pbftSyncedQueueEmpty()) {
    PbftBlockCert pbft_block_and_votes = pbft_chain_->pbftSyncedQueueFront();
    LOG(log_dg_) << "Pick pbft block " << pbft_block_and_votes.pbft_blk->getBlockHash()
                 << " from synced queue in round " << getPbftRound();
    if (pbft_chain_->findPbftBlockInChain(pbft_block_and_votes.pbft_blk->getBlockHash())) {
      // pushed already from PBFT unverified queue, remove and skip it
      pbft_chain_->pbftSyncedQueuePopFront();

      pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
        LOG(log_dg_) << "PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << " already present in chain.";
        LOG(log_dg_) << "PBFT synced queue still contains " << pbft_synced_queue_size
                     << " synced blocks that could not be pushed.";
      }
      pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;
      continue;
    }
    // Check cert votes validation
    if (!vote_mgr_->pbftBlockHasEnoughValidCertVotes(pbft_block_and_votes, getEligibleVoterCount(),
                                                     sortition_threshold_, TWO_T_PLUS_ONE)) {
      // Failed cert votes validation, flush synced PBFT queue and set since
      // next block validation depends on the current one
      LOG(log_er_) << "Synced PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash()
                   << " doesn't have enough valid cert votes. Clear synced PBFT blocks!"
                   << " Eligible voter count: " << getEligibleVoterCount();
      pbft_chain_->clearSyncedPbftBlocks();
      break;
    }
    if (!pbft_chain_->checkPbftBlockValidation(*pbft_block_and_votes.pbft_blk)) {
      // PBFT chain syncing faster than DAG syncing, wait!
      pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
        LOG(log_dg_) << "PBFT chain unable to push synced block " << pbft_block_and_votes.pbft_blk->getBlockHash();
        LOG(log_dg_) << "PBFT synced queue still contains " << pbft_synced_queue_size
                     << " synced blocks that could not be pushed.";
      }
      pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;
      break;
    }
    if (!comparePbftBlockScheduleWithDAGblocks_(*pbft_block_and_votes.pbft_blk)) {
      break;
    }
    if (!pushPbftBlock_(pbft_block_and_votes)) {
      LOG(log_er_) << "Failed push PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << " into chain";
      break;
    }
    // Remove from PBFT synced queue
    pbft_chain_->pbftSyncedQueuePopFront();
    if (executed_pbft_block_) {
      update_dpos_state_();
      // update sortition_threshold and TWO_T_PLUS_ONE
      updateTwoTPlusOneAndThreshold_();
      db_->savePbftMgrStatus(PbftMgrStatus::executed_block, false);
      executed_pbft_block_ = false;
    }
    pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
    if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
      LOG(log_dg_) << "PBFT synced queue still contains " << pbft_synced_queue_size
                   << " synced blocks that could not be pushed.";
    }
    pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;

    // Since we pushed via syncing we should reset this...
    auto batch = db_->createWriteBatch();
    db_->addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_block_in_previous_round, false, batch);
    db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::next_voted_block_hash_in_previous_round, NULL_BLOCK_HASH,
                                     batch);
    db_->commitWriteBatch(batch);
    next_voted_block_from_previous_round_ = std::make_pair(NULL_BLOCK_HASH, false);
  }
}

bool PbftManager::pushPbftBlock_(PbftBlockCert const &pbft_block_cert_votes) {
  auto const &pbft_block_hash = pbft_block_cert_votes.pbft_blk->getBlockHash();
  if (db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_er_) << "PBFT block: " << pbft_block_hash << " in DB already.";
    return false;
  }

  auto pbft_block = pbft_block_cert_votes.pbft_blk;
  auto const &cert_votes = pbft_block_cert_votes.cert_votes;
  auto pbft_period = pbft_block->getPeriod();

  auto batch = db_->createWriteBatch();
  // Add cert votes in DB
  db_->addPbftCertVotesToBatch(pbft_block_hash, cert_votes, batch);
  LOG(log_nf_) << "Storing cert votes of pbft blk " << pbft_block_hash << "\n" << cert_votes;
  // Add period_pbft_block in DB
  db_->addPbftBlockPeriodToBatch(pbft_period, pbft_block_hash, batch);
  // Add PBFT block in DB
  db_->addPbftBlockToBatch(*pbft_block, batch);
  // update PBFT chain size
  pbft_chain_->updatePbftChain(pbft_block_hash);
  // Update PBFT chain head block
  db_->addPbftHeadToBatch(pbft_chain_->getHeadHash(), pbft_chain_->getJsonStr(), batch);

  // Set DAG blocks period
  auto const &anchor_hash = pbft_block->getPivotDagBlockHash();
  auto finalized_dag_blk_hashes = *dag_mgr_->getDagBlockOrder(anchor_hash).second;
  dag_mgr_->setDagBlockOrder(anchor_hash, pbft_period, finalized_dag_blk_hashes, batch);

  // Add dag_block_period in DB
  for (auto const &blk_hash : finalized_dag_blk_hashes) {
    db_->addDagBlockPeriodToBatch(blk_hash, pbft_period, batch);
  }

  // Commit DB
  db_->commitWriteBatch(batch);
  LOG(log_nf_) << node_addr_ << " successful push unexecuted pbft block " << pbft_block_hash << " in period "
               << pbft_period << " into chain! In round " << getPbftRound();

  executor_->execute(pbft_block);

  // Update pbft chain last block hash
  pbft_chain_last_block_hash_ = pbft_block_hash;
  assert(pbft_chain_last_block_hash_ == pbft_chain_->getLastPbftBlockHash());

  // Reset proposed PBFT block hash to False for next pbft block proposal
  proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);
  db_->savePbftMgrStatus(PbftMgrStatus::executed_block, true);
  executed_pbft_block_ = true;
  return true;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  // Update 2t+1 and threshold
  auto eligible_voter_count = getEligibleVoterCount();
  sortition_threshold_ = std::min<size_t>(COMMITTEE_SIZE, eligible_voter_count);
  TWO_T_PLUS_ONE = sortition_threshold_ * 2 / 3 + 1;
  LOG(log_nf_) << "Committee size " << COMMITTEE_SIZE << ", valid voting players " << eligible_voter_count
               << ". Update 2t+1 " << TWO_T_PLUS_ONE << ", Threshold " << sortition_threshold_;
}

void PbftManager::countVotes_() {
  auto round = getPbftRound();
  while (!monitor_stop_) {
    std::vector<Vote> votes = vote_mgr_->getAllVotes();

    size_t last_step_votes = 0;
    size_t current_step_votes = 0;
    for (auto const &v : votes) {
      if (step_ == 1) {
        if (v.getRound() == round - 1 && v.getStep() == last_step_) {
          last_step_votes++;
        } else if (v.getRound() == round && v.getStep() == step_) {
          current_step_votes++;
        }
      } else {
        if (v.getRound() == round) {
          if (v.getStep() == step_ - 1) {
            last_step_votes++;
          } else if (v.getStep() == step_) {
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

}  // namespace taraxa
