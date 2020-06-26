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

#include "dag.hpp"
#include "full_node.hpp"
#include "network.hpp"
#include "sortition.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {
using vrf_output_t = vrf_wrapper::vrf_output_t;

struct ReplayProtectionServiceDummy : ReplayProtectionService {
  bool is_nonce_stale(addr_t const &addr, uint64_t nonce) const override {
    return false;
  }
  void update(DbStorage::BatchPtr batch, round_t round,
              util::RangeView<TransactionInfo> const &trxs) override {}
};

PbftManager::PbftManager(PbftConfig const &conf, std::string const &genesis,
                         addr_t node_addr)
    : replay_protection_service(new ReplayProtectionServiceDummy),
      LAMBDA_ms_MIN(conf.lambda_ms_min),
      COMMITTEE_SIZE(conf.committee_size),
      VALID_SORTITION_COINS(conf.valid_sortition_coins),
      DAG_BLOCKS_SIZE(conf.dag_blocks_size),
      GHOST_PATH_MOVE_BACK(conf.ghost_path_move_back),
      SKIP_PERIODS(conf.skip_periods),
      RUN_COUNT_VOTES(conf.run_count_votes),
      dag_genesis_(genesis) {
  LOG_OBJECTS_CREATE("PBFT_MGR");
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setFullNode(shared_ptr<taraxa::FullNode> full_node) {
  node_ = full_node;
  vote_mgr_ = full_node->getVoteManager();
  pbft_chain_ = full_node->getPbftChain();
  capability_ = full_node->getNetwork()->getTaraxaCapability();
  db_ = full_node->getDB();
  num_executed_blk_ =
      db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_trx_ =
      db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  auto expected_max_trx_per_block =
      full_node->getConfig()
          .opts_final_chain.state_api.ExpectedMaxNumTrxPerBlock;
  dag_block_proposers_tmp_.reserve(expected_max_trx_per_block / 4);
  transactions_tmp_.reserve(expected_max_trx_per_block);
  trx_senders_tmp_.reserve(expected_max_trx_per_block);
}

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  auto full_node = node_.lock();
  std::vector<std::string> ghost;
  full_node->getGhostPath(dag_genesis_, ghost);
  while (ghost.empty()) {
    LOG(log_dg_)
        << "GHOST is empty. DAG initialization has not done. Sleep 100ms";
    thisThreadSleepForMilliSeconds(100);
  }
  LOG(log_dg_) << "PBFT start at GHOST size " << ghost.size()
               << ", the last of DAG blocks is " << ghost.back();

  if (!db_->sortitionAccountInDb(std::string("sortition_accounts_size"))) {
    // New node
    // Initialize master boot node account balance
    addr_t master_boot_node_address = full_node->getMasterBootNodeAddress();
    std::pair<val_t, bool> master_boot_node_account_balance =
        full_node->getBalance(master_boot_node_address);
    if (!master_boot_node_account_balance.second) {
      LOG(log_er_) << "Failed initial master boot node account balance."
                   << " Master boot node balance is not exist.";
    } else if (master_boot_node_account_balance.first != TARAXA_COINS_DECIMAL) {
      LOG(log_nf_)
          << "Initial master boot node account balance. Current balance "
          << master_boot_node_account_balance.first;
    }
    PbftSortitionAccount master_boot_node(
        master_boot_node_address, master_boot_node_account_balance.first, 0,
        new_change);
    sortition_account_balance_table_tmp[master_boot_node_address] =
        master_boot_node;
    auto batch = db_->createWriteBatch();
    updateSortitionAccountsDB_(batch);
    db_->commitWriteBatch(batch);
    updateSortitionAccountsTable_();
  } else {
    // Full node join back
    if (!sortition_account_balance_table.empty()) {
      LOG(log_er_) << "PBFT sortition accounts table should be empty";
      assert(false);
    }
    size_t sortition_accounts_size_db = 0;
    db_->forEachSortitionAccount([&](auto const &key, auto const &value) {
      if (key.ToString() == "sortition_accounts_size") {
        std::stringstream sstream(value.ToString());
        sstream >> sortition_accounts_size_db;
        return true;
      }
      PbftSortitionAccount account(value.ToString());
      sortition_account_balance_table_tmp[account.address] = account;
      return true;
    });
    updateSortitionAccountsTable_();
    assert(sortition_accounts_size_db == valid_sortition_accounts_size_);
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

blk_hash_t PbftManager::getLastPbftBlockHashAtStartOfRound() const {
  return pbft_chain_last_block_hash_;
}

std::pair<bool, uint64_t> PbftManager::getDagBlockPeriod(
    blk_hash_t const &hash) {
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

std::string PbftManager::getScheduleBlockByPeriod(uint64_t const period) {
  auto value = db_->getPeriodPbftBlock(period);
  if (value) {
    blk_hash_t pbft_block_hash = *value;
    return pbft_chain_->getPbftBlockInChain(pbft_block_hash).getJsonStr();
  }
  return "";
}

size_t PbftManager::getSortitionThreshold() const {
  return sortition_threshold_;
}

size_t PbftManager::getTwoTPlusOne() const { return TWO_T_PLUS_ONE; }

void PbftManager::setTwoTPlusOne(size_t const two_t_plus_one) {
  TWO_T_PLUS_ONE = two_t_plus_one;
}

// Notice: Test purpose
void PbftManager::setSortitionThreshold(size_t const sortition_threshold) {
  sortition_threshold_ = sortition_threshold;
}

size_t PbftManager::getValidSortitionAccountsSize() const {
  return valid_sortition_accounts_size_;
}
// End Test

bool PbftManager::shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_er_) << "Full node unavailable";
    return false;
  }
  addr_t account_address = full_node->getAddress();
  if (sortition_account_balance_table.find(account_address) ==
      sortition_account_balance_table.end()) {
    LOG(log_tr_) << "Cannot find account " << account_address
                  << " in sortition table. Don't have enough coins to vote";
    return false;
  }
  // only active players are able to vote
  if (!is_active_player_) {
    LOG(log_tr_)
        << "Account " << account_address << " last period seen at "
        << sortition_account_balance_table[account_address].last_period_seen
        << ", as a non-active player";
    return false;
  }

  // compute sortition
  VrfPbftMsg msg(pbft_chain_last_block_hash_, type, round, step);
  VrfPbftSortition vrf_sortition(full_node->getVrfSecretKey(), msg);
  if (!vrf_sortition.canSpeak(sortition_threshold_,
                              valid_sortition_accounts_size_)) {
    LOG(log_tr_) << "Don't get sortition";
    return false;
  }
  return true;
}

void PbftManager::setPbftStep(size_t const pbft_step) {
  last_step_ = step_;
  step_ = pbft_step;

  if (step_ > MAX_STEPS) {
    // Note: We calculate the lambda for a step independently of prior steps
    //       in case missed earlier steps.
    LAMBDA_ms = 100 * LAMBDA_ms_MIN;
    // LAMBDA_ms = LAMBDA_ms_MIN
    //            << (step_ - MAX_STEPS);  // Multiply by 2 each step...
    LOG(log_nf_) << "Surpassed max steps, relaxing lambda to " << LAMBDA_ms
                 << " ms in round " << round_ << ", step " << step_;
  } else {
    LAMBDA_ms = LAMBDA_ms_MIN;
  }
}

void PbftManager::resetStep_() { setPbftStep(1); }

bool PbftManager::resetRound_() {
  bool restart = false;
  // Check if we are synced to the right step ...
  uint64_t consensus_pbft_round = roundDeterminedFromVotes_();
  // Check should be always true...
  assert(consensus_pbft_round >= round_);

  if (consensus_pbft_round > round_) {
    LOG(log_nf_) << "From votes determined round " << consensus_pbft_round;
    round_clock_initial_datetime_ = now_;
    uint64_t local_round = round_;
    round_ = consensus_pbft_round;
    resetStep_();
    state_ = value_proposal_state;
    LOG(log_dg_) << "Advancing clock to pbft round " << round_
                 << ", step 1, and resetting clock.";

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
    next_voted_block_from_previous_round_ =
        nextVotedBlockForRoundAndStep_(votes_, local_round);

    if (executed_pbft_block_) {
      // Update sortition accounts table
      updateSortitionAccountsTable_();
      // reset sortition_threshold and TWO_T_PLUS_ONE
      updateTwoTPlusOneAndThreshold_();
      executed_pbft_block_ = false;
    }

    LAMBDA_ms = LAMBDA_ms_MIN;
    last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
    current_step_clock_initial_datetime_ = std::chrono::system_clock::now();

    // Update pbft chain last block hash at start of new round...
    pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();
    // END ROUND START STATE CHANGE UPDATES

    // p2p connection syncing should cover this situation, sync here for safe
    if (consensus_pbft_round > local_round + 1 &&
        capability_->syncing_ == false) {
      LOG(log_nf_) << "Quorum determined round " << consensus_pbft_round
                   << " > 1 + current round " << local_round
                   << " local round, need to broadcast request for missing "
                      "certified blocks";
      // NOTE: Advance round and step before calling sync to make sure
      //       sync call won't be supressed for being too
      //       recent (ie. same round and step)
      syncPbftChainFromPeers_();
      next_voted_block_from_previous_round_ =
          std::make_pair(NULL_BLOCK_HASH, false);
    }
    // Restart while loop...
    restart = true;
  }
  return restart;
}

void PbftManager::sleep_() {
  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();
  auto time_to_sleep_for_ms = next_step_time_ms_ - elapsed_time_in_round_ms_;
  if (time_to_sleep_for_ms > 0) {
    LOG(log_tr_) << "Time to sleep(ms): " << time_to_sleep_for_ms
                  << " in round " << round_ << ", step " << step_;
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(time_to_sleep_for_ms));
  }
}

void PbftManager::initialState_() {
  // Initial PBFT state
  state_ = value_proposal_state;

  LAMBDA_ms = LAMBDA_ms_MIN;
  STEP_4_DELAY = 2 * LAMBDA_ms;

  round_ = 1;
  step_ = 1;
  last_step_ = 0;
  // Initial last sync request
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  own_starting_value_for_round_ = NULL_BLOCK_HASH;
  soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, false);
  next_voted_block_from_previous_round_ =
      std::make_pair(NULL_BLOCK_HASH, false);
  executed_pbft_block_ = false;
  have_executed_this_round_ = false;
  should_have_cert_voted_in_this_round_ = false;
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;

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
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
  last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::continueFinishPollingState_(size_t step) {
  state_ = finish_polling_state;
  setPbftStep(step);
  next_voted_soft_value_ = false;
  next_voted_null_block_hash_ = false;
}

void PbftManager::loopBackFinishState_() {
  LOG(log_dg_) << "CONSENSUSDBG round " << round_ << " , step " << step_
               << " | next_voted_soft_value_ = " << next_voted_soft_value_
               << " soft block = " << soft_voted_block_for_this_round_
               << " next_voted_null_block_hash_ = "
               << next_voted_null_block_hash_
               << " next_voted_block_from_previous_round_ = "
               << next_voted_block_from_previous_round_ << " cert voted = "
               << (cert_voted_values_for_round_.find(round_) !=
                   cert_voted_values_for_round_.end());
  state_ = finish_state;
  setPbftStep(step_ + 1);
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
  // update pbft chain last block hash
  pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();

  now_ = std::chrono::system_clock::now();
  duration_ = now_ - round_clock_initial_datetime_;
  elapsed_time_in_round_ms_ =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();

  LOG(log_tr_) << "PBFT current round is " << round_;
  LOG(log_tr_) << "PBFT current step is " << step_;

  // Get votes
  bool sync_peers_pbft_chain = false;
  votes_ = vote_mgr_->getVotes(round_, valid_sortition_accounts_size_,
                               sync_peers_pbft_chain);
  LOG(log_tr_) << "There are " << votes_.size() << " total votes in round "
                << round_;

  // Concern can malicious node trigger excessive syncing?
  if (sync_peers_pbft_chain && pbft_chain_->pbftSyncedQueueEmpty() &&
      !capability_->syncing_ && !syncRequestedAlreadyThisStep_()) {
    LOG(log_nf_) << "Vote validation triggered PBFT chain sync";
    syncPbftChainFromPeers_();
  }

  // CHECK IF WE HAVE RECEIVED 2t+1 CERT VOTES FOR A BLOCK IN OUR CURRENT
  // ROUND.  IF WE HAVE THEN WE EXECUTE THE BLOCK
  // ONLY CHECK IF HAVE *NOT* YET EXECUTED THIS ROUND...
  if ((state_ == certify_state || state_ == finish_state) &&
      !have_executed_this_round_) {
    std::vector<Vote> cert_votes_for_round =
        getVotesOfTypeFromVotesForRoundAndStep_(
            cert_vote_type, votes_, round_, 3,
            std::make_pair(NULL_BLOCK_HASH, false));
    std::pair<blk_hash_t, bool> cert_voted_block_hash =
        blockWithEnoughVotes_(cert_votes_for_round);
    if (cert_voted_block_hash.second) {
      LOG(log_dg_) << "PBFT block " << cert_voted_block_hash.first
                   << " has enough certed votes";
      // put pbft block into chain
      if (pushCertVotedPbftBlockIntoChain_(cert_voted_block_hash.first,
                                           cert_votes_for_round)) {
        push_block_values_for_round_[round_] = cert_voted_block_hash.first;
        have_executed_this_round_ = true;
        LOG(log_nf_) << "Write " << cert_votes_for_round.size()
                     << " votes ... in round " << round_;
        duration_ = std::chrono::system_clock::now() - now_;
        auto execute_trxs_in_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(duration_)
                .count();
        LOG(log_dg_) << "Pushing PBFT block and Execution spent "
                     << execute_trxs_in_ms << " ms. in round " << round_;
        // Restart while loop
        return true;
      }
    }
  }
  // We skip step 4 due to having missed it while executing....
  if (state_ == certify_state && have_executed_this_round_ &&
      elapsed_time_in_round_ms_ >
          4 * LAMBDA_ms + STEP_4_DELAY + 2 * POLLING_INTERVAL_ms) {
    LOG(log_dg_)
        << "Skipping step 4 due to execution, will go to step 5 in round "
        << round_;
    step_ = 5;
    state_ = finish_polling_state;
  }

  return resetRound_();
}

void PbftManager::proposeBlock_() {
  // Value Proposal
  LOG(log_tr_) << "PBFT value proposal state in round " << round_;
  if (next_voted_block_from_previous_round_.second) {
    LOG(log_nf_) << "We have a next voted block from previous round "
                 << round_ - 1;
    if (next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH) {
      LOG(log_nf_) << "Previous round next voted block is NULL_BLOCK_HASH";
    } else {
      LOG(log_nf_) << "Previous round next voted block is NOT NULL_BLOCK_HASH";
    }
  } else {
    LOG(log_nf_) << "No next voted block from previous round " << round_ - 1;
  }

  if (round_ == 1) {
    if (shouldSpeak(propose_vote_type, round_, step_)) {
      LOG(log_nf_) << "Proposing value of NULL_BLOCK_HASH " << NULL_BLOCK_HASH
                   << " for round 1 by protocol";
      placeVote_(own_starting_value_for_round_, propose_vote_type, round_,
                 step_);
    }
  } else if (push_block_values_for_round_.count(round_ - 1) ||
             (round_ >= 2 && next_voted_block_from_previous_round_.second &&
              next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH)) {
    if (shouldSpeak(propose_vote_type, round_, step_)) {
      // PBFT block only be proposed once in one period
      if (!proposed_block_hash_.second ||
          proposed_block_hash_.first == NULL_BLOCK_HASH) {
        // Propose value...
        proposed_block_hash_ = proposeMyPbftBlock_();
      }
      if (proposed_block_hash_.second) {
        own_starting_value_for_round_ = proposed_block_hash_.first;
        LOG(log_nf_) << "Proposing own starting value "
                     << own_starting_value_for_round_ << " for round "
                     << round_;
        placeVote_(proposed_block_hash_.first, propose_vote_type, round_,
                   step_);
      }
    }
  } else if (round_ >= 2) {
    if (next_voted_block_from_previous_round_.second &&
        next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
      own_starting_value_for_round_ =
          next_voted_block_from_previous_round_.first;
      if (shouldSpeak(propose_vote_type, round_, step_)) {
        LOG(log_nf_) << "Proposing next voted block "
                     << next_voted_block_from_previous_round_.first
                     << " from previous round, for round " << round_;
        placeVote_(next_voted_block_from_previous_round_.first,
                   propose_vote_type, round_, step_);
      }
    }
  }
}

void PbftManager::identifyBlock_() {
  // The Filtering Step
  LOG(log_tr_) << "PBFT filtering state in round " << round_;
  if (round_ == 1 || push_block_values_for_round_.count(round_ - 1) ||
      (round_ >= 2 && next_voted_block_from_previous_round_.second &&
       next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH)) {
    // Identity leader
    std::pair<blk_hash_t, bool> leader_block = identifyLeaderBlock_(votes_);
    if (leader_block.second) {
      own_starting_value_for_round_ = leader_block.first;
      LOG(log_dg_) << "Identify leader block " << leader_block.first
                   << " at round " << round_;
      if (shouldSpeak(soft_vote_type, round_, step_)) {
        LOG(log_dg_) << "Soft voting block " << leader_block.first
                     << " at round " << round_;
        placeVote_(leader_block.first, soft_vote_type, round_, step_);
      }
    }
  } else if (round_ >= 2 && next_voted_block_from_previous_round_.second &&
             next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
    if (shouldSpeak(soft_vote_type, round_, step_)) {
      LOG(log_dg_) << "Soft voting "
                   << next_voted_block_from_previous_round_.first
                   << " from previous round";
      placeVote_(next_voted_block_from_previous_round_.first, soft_vote_type,
                 round_, step_);
    }
  }
}

void PbftManager::certifyBlock_() {
  // The Certifying Step
  LOG(log_tr_) << "PBFT certifying state in round " << round_;
  if (elapsed_time_in_round_ms_ < 2 * LAMBDA_ms) {
    // Should not happen, add log here for safety checking
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only "
                 << elapsed_time_in_round_ms_ << " (ms) in round " << round_;
  }

  go_finish_state_ = elapsed_time_in_round_ms_ >
                     4 * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms;
  if (go_finish_state_) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in round " << round_;
  } else if (!should_have_cert_voted_in_this_round_) {
    LOG(log_tr_) << "In step 3";
    if (!soft_voted_block_for_this_round_.second) {
      soft_voted_block_for_this_round_ =
          softVotedBlockForRound_(votes_, round_);
    }
    if (soft_voted_block_for_this_round_.second &&
        soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH &&
        comparePbftBlockScheduleWithDAGblocks_(
            soft_voted_block_for_this_round_.first)) {
      LOG(log_tr_) << "Finished comparePbftBlockScheduleWithDAGblocks_";

      // NOTE: If we have already executed this round
      //       then block won't be found in unverified queue...
      bool executed_soft_voted_block_for_this_round = false;
      if (have_executed_this_round_) {
        LOG(log_tr_)
            << "Have already executed before certifying in step 3 in round "
            << round_;
        if (pbft_chain_->getLastPbftBlockHash() ==
            soft_voted_block_for_this_round_.first) {
          LOG(log_tr_) << "Having executed, last block in chain is the "
                           "soft voted block in round "
                        << round_;
          executed_soft_voted_block_for_this_round = true;
        }
      }

      bool unverified_soft_vote_block_for_this_round_is_valid = false;
      if (!executed_soft_voted_block_for_this_round) {
        if (checkPbftBlockValid_(soft_voted_block_for_this_round_.first)) {
          LOG(log_tr_) << "checkPbftBlockValid_ returned true";
          unverified_soft_vote_block_for_this_round_is_valid = true;
        } else {
          // Get partition, need send request to get missing pbft blocks
          // from peers
          LOG(log_er_)
              << "Soft voted block for this round appears to be invalid, "
                 "we must be out of sync with pbft chain";
          if (!capability_->syncing_) {
            syncPbftChainFromPeers_();
          }
        }
      }

      if (executed_soft_voted_block_for_this_round ||
          unverified_soft_vote_block_for_this_round_is_valid) {
        cert_voted_values_for_round_[round_] =
            soft_voted_block_for_this_round_.first;

        // NEED TO KEEP POLLING TO SEE IF WE HAVE 2t+1 cert votes...
        // Here we would cert vote if we can speak....
        should_have_cert_voted_in_this_round_ = true;
        if (shouldSpeak(cert_vote_type, round_, step_)) {
          LOG(log_dg_) << "Cert voting "
                       << soft_voted_block_for_this_round_.first
                       << " for round " << round_;
          // generate cert vote
          placeVote_(soft_voted_block_for_this_round_.first, cert_vote_type,
                     round_, step_);
        }
      }
    }
  }
}

void PbftManager::firstFinish_() {
  // Even number steps from 4 are in first finish
  LOG(log_tr_) << "PBFT first finishing state at step " << step_
                << " in round " << round_;
  if (shouldSpeak(next_vote_type, round_, step_)) {
    if (cert_voted_values_for_round_.find(round_) !=
        cert_voted_values_for_round_.end()) {
      LOG(log_dg_) << "Next voting cert voted value "
                   << cert_voted_values_for_round_[round_] << " for round "
                   << round_ << " , step " << step_;
      placeVote_(cert_voted_values_for_round_[round_], next_vote_type, round_,
                 step_);
    } else if (round_ >= 2 && next_voted_block_from_previous_round_.second &&
               next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH) {
      LOG(log_dg_) << "Next voting NULL BLOCK for round " << round_
                   << ", at step " << step_;
      placeVote_(NULL_BLOCK_HASH, next_vote_type, round_, step_);
    } else {
      LOG(log_dg_) << "Next voting nodes own starting value "
                   << own_starting_value_for_round_ << " for round " << round_
                   << ", at step " << step_;
      placeVote_(own_starting_value_for_round_, next_vote_type, round_, step_);
    }
  }
}

void PbftManager::secondFinish_() {
  // Odd number steps from 5 are in second finish
  LOG(log_tr_) << "PBFT second finishing state at step " << step_
                << " in round " << round_;
  long end_time_for_step =
      (step_ + 1) * LAMBDA_ms + STEP_4_DELAY + 2 * POLLING_INTERVAL_ms;
  if (step_ > MAX_STEPS) {
    u_long LAMBDA_ms_BIG = 100 * LAMBDA_ms_MIN;
    end_time_for_step = MAX_STEPS * LAMBDA_ms_MIN +
                        (step_ - MAX_STEPS + 1) * LAMBDA_ms_BIG + STEP_4_DELAY +
                        2 * POLLING_INTERVAL_ms;
  }
  if (elapsed_time_in_round_ms_ > end_time_for_step) {
    // Should not happen, add log here for safety checking
    if (have_executed_this_round_) {
      LOG(log_dg_) << "PBFT Reached round " << round_ << " at step " << step_
                   << " late due to execution";
    } else {
      LOG(log_dg_) << "PBFT Reached round " << round_ << " at step " << step_
                   << " late without executing";
    }
    continue_finish_polling_state_ = true;
    return;
  }

  if (shouldSpeak(next_vote_type, round_, step_)) {
    if (!soft_voted_block_for_this_round_.second) {
      soft_voted_block_for_this_round_ =
          softVotedBlockForRound_(votes_, round_);
    }
    if (!next_voted_soft_value_ && soft_voted_block_for_this_round_.second &&
        soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH) {
      LOG(log_dg_) << "Next voting " << soft_voted_block_for_this_round_.first
                   << " for round " << round_ << ", at step " << step_;
      placeVote_(soft_voted_block_for_this_round_.first, next_vote_type, round_,
                 step_);
      next_voted_soft_value_ = true;
    }
    if (!next_voted_null_block_hash_ && round_ >= 2 &&
        next_voted_block_from_previous_round_.second &&
        // next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH &&
        (cert_voted_values_for_round_.find(round_) ==
         cert_voted_values_for_round_.end())) {
      LOG(log_dg_) << "Next voting NULL BLOCK for round " << round_
                   << ", at step " << step_;
      placeVote_(NULL_BLOCK_HASH, next_vote_type, round_, step_);
      next_voted_null_block_hash_ = true;
    }
  }

  if (step_ > MAX_STEPS && !capability_->syncing_ &&
      !syncRequestedAlreadyThisStep_()) {
    LOG(log_wr_) << "Suspect PBFT chain behind, inaccurate 2t+1, need "
                     "to broadcast request for missing blocks";
    syncPbftChainFromPeers_();
  }

  loop_back_finish_state_ =
      elapsed_time_in_round_ms_ >
      (step_ + 1) * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms;
}

// There is a quorum of next-votes and set determine that round p should be the
// current round...
uint64_t PbftManager::roundDeterminedFromVotes_() {
  // <<vote_round, vote_step>, count>, <round, step> store in reverse order
  std::map<std::pair<uint64_t, size_t>, size_t,
           std::greater<std::pair<uint64_t, size_t>>>
      next_votes_tally_by_round_step;

  for (Vote &v : votes_) {
    if (v.getType() != next_vote_type) {
      continue;
    }

    std::pair<uint64_t, size_t> round_step =
        std::make_pair(v.getRound(), v.getStep());
    if (round_step.first >= round_) {
      if (next_votes_tally_by_round_step.find(round_step) !=
          next_votes_tally_by_round_step.end()) {
        next_votes_tally_by_round_step[round_step] += 1;
      } else {
        next_votes_tally_by_round_step[round_step] = 1;
      }
    }
  }

  for (auto &rs_votes : next_votes_tally_by_round_step) {
    if (rs_votes.second >= TWO_T_PLUS_ONE) {
      std::vector<Vote> next_votes_for_round_step =
          getVotesOfTypeFromVotesForRoundAndStep_(
              next_vote_type, votes_, rs_votes.first.first,
              rs_votes.first.second, std::make_pair(NULL_BLOCK_HASH, false));
      if (blockWithEnoughVotes_(next_votes_for_round_step).second) {
        LOG(log_dg_) << "Found sufficient next votes in round "
                     << rs_votes.first.first << ", step "
                     << rs_votes.first.second;
        return rs_votes.first.first + 1;
      }
    }
  }
  return round_;
}

// Assumption is that all votes are in the same round and of same type...
std::pair<blk_hash_t, bool> PbftManager::blockWithEnoughVotes_(
    std::vector<Vote> const &votes) const {
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
      bool vote_type_and_round_is_consistent =
          (vote_type == v.getType() && vote_round == v.getRound());
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
        LOG(log_dg_) << "find block hash " << blockhash_pair.first
                     << " vote type " << vote_type << " in round " << vote_round
                     << " has " << blockhash_pair.second << " votes";
        return std::make_pair(blockhash_pair.first, true);
      } else {
        LOG(log_tr_) << "Don't have enough votes. block hash "
                      << blockhash_pair.first << " vote type " << vote_type
                      << " in round " << vote_round << " has "
                      << blockhash_pair.second << " votes"
                      << " (2TP1 = " << TWO_T_PLUS_ONE << ")";
      }
    }
  }

  return std::make_pair(NULL_BLOCK_HASH, false);
}

std::map<size_t, std::vector<Vote>, std::greater<size_t>>
PbftManager::getVotesOfTypeFromVotesForRoundByStep_(
    PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
    std::pair<blk_hash_t, bool> blockhash) {
  //<vote_step, votes> reverse order
  std::map<size_t, std::vector<Vote>, std::greater<size_t>>
      requested_votes_by_step;

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

std::vector<Vote> PbftManager::getVotesOfTypeFromVotesForRoundAndStep_(
    PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
    size_t step, std::pair<blk_hash_t, bool> blockhash) {
  std::vector<Vote> votes_of_requested_type;
  std::copy_if(votes.begin(), votes.end(),
               std::back_inserter(votes_of_requested_type),
               [vote_type, round, step, blockhash](Vote const &v) {
                 return (v.getType() == vote_type && v.getRound() == round &&
                         v.getStep() == step &&
                         (blockhash.second == false ||
                          blockhash.first == v.getBlockHash()));
               });

  return votes_of_requested_type;
}

std::pair<blk_hash_t, bool> PbftManager::nextVotedBlockForRoundAndStep_(
    std::vector<Vote> &votes, uint64_t round) {
  //<vote_step, votes> reverse order
  std::map<size_t, std::vector<Vote>, std::greater<size_t>>
      next_votes_in_round_by_step = getVotesOfTypeFromVotesForRoundByStep_(
          next_vote_type, votes, round, std::make_pair(NULL_BLOCK_HASH, false));

  std::pair<blk_hash_t, bool> next_vote_block_hash =
      std::make_pair(NULL_BLOCK_HASH, false);
  for (auto &sv : next_votes_in_round_by_step) {
    next_vote_block_hash = blockWithEnoughVotes_(sv.second);
    if (next_vote_block_hash.second) {
      return next_vote_block_hash;
    }
  }
  return next_vote_block_hash;
}

void PbftManager::placeVote_(taraxa::blk_hash_t const &blockhash,
                             PbftVoteTypes vote_type, uint64_t round,
                             size_t step) {
  auto full_node = node_.lock();
  Vote vote = full_node->generateVote(blockhash, vote_type, round, step,
                                      pbft_chain_last_block_hash_);
  vote_mgr_->addVote(vote);
  LOG(log_dg_) << "vote block hash: " << blockhash
               << " vote type: " << vote_type << " round: " << round
               << " step: " << step << " vote hash " << vote.getHash();
  // pbft vote broadcast
  full_node->broadcastVote(vote);
}

std::pair<blk_hash_t, bool> PbftManager::softVotedBlockForRound_(
    std::vector<taraxa::Vote> &votes, uint64_t round) {
  std::vector<Vote> soft_votes_for_round =
      getVotesOfTypeFromVotesForRoundAndStep_(
          soft_vote_type, votes, round, 2,
          std::make_pair(NULL_BLOCK_HASH, false));

  return blockWithEnoughVotes_(soft_votes_for_round);
}

std::pair<blk_hash_t, bool> PbftManager::proposeMyPbftBlock_() {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_er_) << "Full node unavailable" << std::endl;
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  LOG(log_dg_) << "Into propose PBFT block";
  blk_hash_t last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  PbftBlock last_pbft_block;
  std::string last_period_dag_anchor_block_hash;
  if (last_pbft_block_hash) {
    last_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    last_period_dag_anchor_block_hash =
        last_pbft_block.getPivotDagBlockHash().toString();
  } else {
    // First PBFT pivot block
    last_period_dag_anchor_block_hash = dag_genesis_;
  }

  std::vector<std::string> ghost;
  full_node->getGhostPath(last_period_dag_anchor_block_hash, ghost);
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
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis "
                 << dag_block_hash << " PBFT propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // compare with last dag block hash. If they are same, which means no new
  // dag blocks generated since last round. In that case PBFT proposer should
  // propose NULL BLOCK HASH as their value and not produce a new block. In
  // practice this should never happen
  if (dag_block_hash.toString() == last_period_dag_anchor_block_hash) {
    LOG(log_dg_)
        << "Last period DAG anchor block hash " << dag_block_hash
        << " No new DAG blocks generated, PBFT propose NULL_BLOCK_HASH";
    LOG(log_dg_) << "Ghost: " << ghost;
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // get dag blocks hash order
  uint64_t period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // get dag blocks
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  for (auto const &dag_blk_hash : *dag_blocks_hash_order) {
    auto dag_blk = full_node->getDagBlock(dag_blk_hash);
    assert(dag_blk);
    dag_blocks.emplace_back(dag_blk);
  }

  std::vector<std::vector<std::pair<trx_hash_t, uint>>> dag_blocks_trxs_mode;
  std::unordered_set<trx_hash_t> unique_trxs;
  auto final_chain = full_node->getFinalChain();
  for (auto const &dag_blk : dag_blocks) {
    // get transactions for each DAG block
    auto trxs_hash = dag_blk->getTrxs();
    std::vector<std::pair<trx_hash_t, uint>> each_dag_blk_trxs_mode;
    for (auto const &t_hash : trxs_hash) {
      if (unique_trxs.find(t_hash) != unique_trxs.end()) {
        // Duplicate transations
        continue;
      }
      unique_trxs.insert(t_hash);
      auto trx = db_->getTransaction(t_hash);
      if (!final_chain->isKnownTransaction(t_hash) &&
          !replay_protection_service->is_nonce_stale(trx->getSender(),
                                                     trx->getNonce())) {
        // TODO: Generate fake transaction schedule, will need pass to VM to
        //  generate the transaction schedule later
        each_dag_blk_trxs_mode.emplace_back(std::make_pair(t_hash, 1));
      }
    }
    dag_blocks_trxs_mode.emplace_back(each_dag_blk_trxs_mode);
  }

  //  TODO: Keep for now to compare, will remove later
  //  // get transactions overlap table
  //  std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
  //      trx_overlap_table =
  //          full_node->computeTransactionOverlapTable(dag_blocks_hash_order);
  //  if (!trx_overlap_table) {
  //    LOG(log_er_) << "Transaction overlap table nullptr, cannot create mock
  //    "
  //                  << "transactions schedule";
  //    return std::make_pair(NULL_BLOCK_HASH, false);
  //  }
  //  if (trx_overlap_table->empty()) {
  //    LOG(log_dg_) << "Transaction overlap table is empty, no DAG block needs
  //    "
  //                  << " generate mock trx schedule";
  //    return std::make_pair(NULL_BLOCK_HASH, false);
  //  }
  //  // TODO: generate fake transaction schedule for now, will pass
  //  //  trx_overlap_table to VM
  //  std::vector<std::vector<uint>> dag_blocks_trx_modes =
  //      full_node->createMockTrxSchedule(trx_overlap_table);

  TrxSchedule schedule(*dag_blocks_hash_order, dag_blocks_trxs_mode);
  uint64_t propose_pbft_period = pbft_chain_->getPbftChainSize() + 1;
  addr_t beneficiary = full_node->getAddress();
  // generate generate pbft block
  PbftBlock pbft_block(last_pbft_block_hash, dag_block_hash, schedule,
                       propose_pbft_period, beneficiary,
                       full_node->getSecretKey());
  // push pbft block
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
  // broadcast pbft block
  std::shared_ptr<Network> network = full_node->getNetwork();
  network->onNewPbftBlock(pbft_block);

  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  LOG(log_dg_) << full_node->getAddress() << " propose PBFT block succussful! "
               << " in round: " << round_ << " in step: " << step_
               << " PBFT block: " << pbft_block;
  return std::make_pair(pbft_block_hash, true);
}

std::pair<blk_hash_t, bool> PbftManager::identifyLeaderBlock_(
    std::vector<Vote> const &votes) {
  LOG(log_dg_) << "Into identify leader block, in round " << round_;
  // each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<vrf_output_t, blk_hash_t>> leader_candidates;
  for (auto const &v : votes) {
    if (v.getRound() == round_ && v.getType() == propose_vote_type) {
      // We should not pick any null block as leader (proposed when
      // no new blocks found, or maliciously) if others have blocks.
      if (round_ == 1 || v.getBlockHash() != NULL_BLOCK_HASH) {
        leader_candidates.emplace_back(
            std::make_pair(v.getCredential(), v.getBlockHash()));
      }
    }
  }
  if (leader_candidates.empty()) {
    // no eligible leader
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  std::pair<vrf_output_t, blk_hash_t> leader =
      *std::min_element(leader_candidates.begin(), leader_candidates.end(),
                        [](std::pair<vrf_output_t, blk_hash_t> const &i,
                           std::pair<vrf_output_t, blk_hash_t> const &j) {
                          return i.first < j.first;
                        });

  return std::make_pair(leader.second, true);
}

bool PbftManager::checkPbftBlockValid_(blk_hash_t const &block_hash) const {
  std::pair<PbftBlock, bool> cert_voted_block =
      pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!cert_voted_block.second) {
    LOG(log_er_) << "Cannot find the unverified pbft block, block hash "
                 << block_hash;
    return false;
  }
  return pbft_chain_->checkPbftBlockValidation(cert_voted_block.first);
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return round_ == pbft_round_last_requested_sync_ &&
         step_ == pbft_step_last_requested_sync_;
}

void PbftManager::syncPbftChainFromPeers_() {
  if (!pbft_chain_->pbftSyncedQueueEmpty()) {
    LOG(log_dg_) << "DAG has not synced yet. PBFT chain skips syncing";
    return;
  }

  if (capability_->syncing_ == false) {
    if (syncRequestedAlreadyThisStep_() == false) {
      LOG(log_nf_) << "Restarting pbft sync."
                   << " In round " << round_ << ", in step " << step_
                   << " Send request to ask missing pbft blocks in chain";
      capability_->restartSyncingPbft();
      pbft_round_last_requested_sync_ = round_;
      pbft_step_last_requested_sync_ = step_;
    }
  }
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(
    blk_hash_t const &pbft_block_hash) {
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getUnverifiedPbftBlock(pbft_block_hash);
  if (!pbft_block.second) {
    LOG(log_dg_) << "Have not got the PBFT block yet. block hash: "
                 << pbft_block_hash;
    return false;
  }

  return comparePbftBlockScheduleWithDAGblocks_(pbft_block.first);
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(
    PbftBlock const &pbft_block) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_er_) << "Full node unavailable" << std::endl;
    return false;
  }
  uint64_t last_period = pbft_chain_->getPbftChainSize();
  // get DAG blocks order
  blk_hash_t dag_block_hash = pbft_block.getPivotDagBlockHash();
  uint64_t period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // compare DAG blocks hash in PBFT schedule with DAG blocks
  vec_blk_t dag_blocks_hash_in_schedule =
      pbft_block.getSchedule().dag_blks_order;
  if (dag_blocks_hash_in_schedule.size() == dag_blocks_hash_order->size()) {
    for (auto i = 0; i < dag_blocks_hash_in_schedule.size(); i++) {
      if (dag_blocks_hash_in_schedule[i] != (*dag_blocks_hash_order)[i]) {
        LOG(log_nf_) << "DAG blocks have not sync yet. In period: "
                     << last_period << ", DAG block hash "
                     << dag_blocks_hash_in_schedule[i]
                     << " in PBFT schedule is different with DAG block hash "
                     << (*dag_blocks_hash_order)[i];
        return false;
      }
    }
  } else {
    LOG(log_nf_) << "DAG blocks have not sync yet. In period: " << last_period
                 << " PBFT block schedule DAG blocks size: "
                 << dag_blocks_hash_in_schedule.size()
                 << ", DAG blocks size: " << dag_blocks_hash_order->size();
    // For debug
    if (dag_blocks_hash_order->size() > dag_blocks_hash_in_schedule.size()) {
      LOG(log_er_) << "Print DAG blocks order:";
      for (auto const &block : *dag_blocks_hash_order) {
        LOG(log_er_) << "block: " << block;
      }
      LOG(log_er_) << "Print DAG blocks in PBFT schedule order:";
      for (auto const &block : dag_blocks_hash_in_schedule) {
        LOG(log_er_) << "block: " << block;
      }
      std::string filename = "unmatched_pbft_schedule_order_in_period_" +
                             std::to_string(last_period);
      auto addr = full_node->getAddress();
      full_node->drawGraph(addr.toString() + "_" + filename);
      // assert(false);
    }
    return false;
  }

  //  // TODO: may not need to compare transactions, keep it now. If need to
  //  //  compare transactions will need to compare one by one. Would cause
  //  //  performance issue. Below code need to modify.
  //  // compare number of transactions in CS with DAG blocks
  //  // PBFT CS block number of transactions
  //  std::vector<std::vector<uint>> trx_modes =
  //      pbft_block_cs.getScheduleBlock().getSchedule().trxs_mode;
  //  for (int i = 0; i < dag_blocks_hash_order->size(); i++) {
  //    std::shared_ptr<DagBlock> dag_block =
  //        full_node->getDagBlock((*dag_blocks_hash_order)[i]);
  //    // DAG block transations
  //    vec_trx_t dag_block_trxs = dag_block->getTrxs();
  //    if (trx_modes[i].size() != dag_block_trxs.size()) {
  //      LOG(log_er_) << "In DAG block hash: " << (*dag_blocks_hash_order)[i]
  //                    << " has " << dag_block_trxs.size()
  //                    << " transactions. But the DAG block in PBFT CS block "
  //                    << "only has " << trx_modes[i].size() << "
  //                    transactions.";
  //      return false;
  //    }
  //  }

  return true;
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(
    taraxa::blk_hash_t const &cert_voted_block_hash,
    std::vector<Vote> const &cert_votes_for_round) {
  if (!checkPbftBlockValid_(cert_voted_block_hash)) {
    // Get partition, need send request to get missing pbft blocks from peers
    LOG(log_er_) << "Cert voted block " << cert_voted_block_hash
                 << " is invalid, we must be out of sync with pbft chain";
    if (capability_->syncing_ == false) {
      syncPbftChainFromPeers_();
    }
    return false;
  }
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getUnverifiedPbftBlock(cert_voted_block_hash);
  if (!pbft_block.second) {
    LOG(log_er_) << "Can not find the cert vote block hash "
                 << cert_voted_block_hash << " in pbft queue";
    return false;
  }
  if (!comparePbftBlockScheduleWithDAGblocks_(pbft_block.first)) {
    return false;
  }
  if (!pushPbftBlock_(pbft_block.first, cert_votes_for_round)) {
    LOG(log_er_) << "Failed push PBFT block " << pbft_block.first.getBlockHash()
                 << " into chain";
    return false;
  }
  // cleanup PBFT unverified blocks table
  pbft_chain_->cleanupUnverifiedPbftBlocks(pbft_block.first);
  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain_() {
  size_t pbft_synced_queue_size;
  auto full_node = node_.lock();
  while (!pbft_chain_->pbftSyncedQueueEmpty()) {
    PbftBlockCert pbft_block_and_votes = pbft_chain_->pbftSyncedQueueFront();
    LOG(log_dg_) << "Pick pbft block "
                 << pbft_block_and_votes.pbft_blk.getBlockHash()
                 << " from synced queue in round " << round_;
    if (pbft_chain_->findPbftBlockInChain(
            pbft_block_and_votes.pbft_blk.getBlockHash())) {
      // pushed already from PBFT unverified queue, remove and skip it
      pbft_chain_->pbftSyncedQueuePopFront();

      pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
        LOG(log_dg_) << "PBFT block "
                     << pbft_block_and_votes.pbft_blk.getBlockHash()
                     << " already present in chain.";
        LOG(log_dg_) << "PBFT synced queue still contains "
                     << pbft_synced_queue_size
                     << " synced blocks that could not be pushed.";
      }
      pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;
      continue;
    }
    // Check cert votes validation
    if (!vote_mgr_->pbftBlockHasEnoughValidCertVotes(
            pbft_block_and_votes, valid_sortition_accounts_size_,
            sortition_threshold_, TWO_T_PLUS_ONE)) {
      // Failed cert votes validation, flush synced PBFT queue and set since
      // next block validation depends on the current one
      LOG(log_er_)
          << "Synced PBFT block "
          << pbft_block_and_votes.pbft_blk.getBlockHash()
          << " doesn't have enough valid cert votes. Clear synced PBFT blocks!"
          << " Active players " << active_nodes;
      pbft_chain_->clearSyncedPbftBlocks();
      break;
    }
    if (!pbft_chain_->checkPbftBlockValidation(pbft_block_and_votes.pbft_blk)) {
      // PBFT chain syncing faster than DAG syncing, wait!
      pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
        LOG(log_dg_) << "PBFT chain unable to push synced block "
                     << pbft_block_and_votes.pbft_blk.getBlockHash();
        LOG(log_dg_) << "PBFT synced queue still contains "
                     << pbft_synced_queue_size
                     << " synced blocks that could not be pushed.";
      }
      pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;
      break;
    }
    if (!comparePbftBlockScheduleWithDAGblocks_(
            pbft_block_and_votes.pbft_blk)) {
      break;
    }
    if (!pushPbftBlock_(pbft_block_and_votes.pbft_blk,
                        pbft_block_and_votes.cert_votes)) {
      LOG(log_er_) << "Failed push PBFT block "
                   << pbft_block_and_votes.pbft_blk.getBlockHash()
                   << " into chain";
      break;
    }
    // Remove from PBFT synced queue
    pbft_chain_->pbftSyncedQueuePopFront();
    if (executed_pbft_block_) {
      // Update sortition accounts table
      updateSortitionAccountsTable_();
      // update sortition_threshold and TWO_T_PLUS_ONE
      updateTwoTPlusOneAndThreshold_();
      executed_pbft_block_ = false;
    }
    pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
    if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
      LOG(log_dg_) << "PBFT synced queue still contains "
                   << pbft_synced_queue_size
                   << " synced blocks that could not be pushed.";
    }
    pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;

    // Since we pushed via syncing we should reset this...
    next_voted_block_from_previous_round_ =
        std::make_pair(NULL_BLOCK_HASH, false);
  }
}

bool PbftManager::pushPbftBlock_(PbftBlock const &pbft_block,
                                 std::vector<Vote> const &cert_votes) {
  dag_block_proposers_tmp_.clear();
  transactions_tmp_.clear();
  trx_senders_tmp_.clear();
  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  if (db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_er_) << "PBFT block: " << pbft_block_hash << " in DB already";
    return false;
  }

  auto const &schedule = pbft_block.getSchedule();
  auto full_node = node_.lock();
  // Update transaction overlap table (still use the table?)
  full_node->getTrxOrderMgr()->updateOrderedTrx(schedule);

  // Execute PBFT schedule
  auto dag_blk_count = schedule.dag_blks_order.size();
  for (auto blk_i(0); blk_i < dag_blk_count; ++blk_i) {
    auto &blk_hash = schedule.dag_blks_order[blk_i];
    auto dag_block = db_->getDagBlockRaw(blk_hash);
    if (dag_block.empty()) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash;
      LOG(log_er_) << "Failed to execute PBFT schedule. PBFT Block: "
                   << pbft_block;
      for (auto const &v : cert_votes) {
        LOG(log_er_) << "Cert vote: " << v;
      }
      return false;
    }
    dag_block_proposers_tmp_.insert(DagBlock(dag_block).getSender());
    auto &dag_blk_trxs_mode = schedule.trxs_mode[blk_i];
    for (auto &[trx_hash, _] : dag_blk_trxs_mode) {
      auto const &trx_rlp = db_->getTransactionRaw(trx_hash);
      auto &trx = transactions_tmp_.emplace_back(
          trx_rlp, dev::eth::CheckTransaction::None, true, trx_hash);
      trx_senders_tmp_.insert(trx.sender());
      LOG(log_tr_) << "Transaction " << trx_hash
                    << " read from db at: " << getCurrentTimeMilliSeconds();
    }
  }
  auto batch = db_->createWriteBatch();
  // Execute transactions in EVM(GO trx engine) and update Ethereum block
  auto const &[new_eth_header, trx_receipts, state_transition_result] =
      full_node->getFinalChain()->advance(batch, pbft_block.getBeneficiary(),
                                          pbft_block.getTimestamp(),
                                          transactions_tmp_);

  uint64_t pbft_period = pbft_block.getPeriod();
  // Update replay protection service, like nonce watermark. Nonce watermark has
  // been disabled
  replay_protection_service->update(
      batch, pbft_period,
      util::make_range_view(transactions_tmp_).map([](auto const &trx) {
        return ReplayProtectionService::TransactionInfo{
            trx.from(),
            trx.nonce(),
        };
      }));
  if (dag_blk_count != 0) {
    num_executed_blk_.fetch_add(dag_blk_count);
    num_executed_trx_.fetch_add(transactions_tmp_.size());
    db_->addStatusFieldToBatch(StatusDbField::ExecutedBlkCount,
                               num_executed_blk_, batch);
    db_->addStatusFieldToBatch(StatusDbField::ExecutedTrxCount,
                               num_executed_trx_, batch);
    LOG(log_nf_) << full_node->getAddress() << " :   Executed dag blocks #"
                 << num_executed_blk_ - dag_blk_count << "-"
                 << num_executed_blk_ - 1
                 << " , Transactions count: " << transactions_tmp_.size();
  }

  // Update temp sortition accounts table
  updateTempSortitionAccountsTable_(
      pbft_period, dag_block_proposers_tmp_, trx_senders_tmp_,
      state_transition_result.NonContractBalanceChanges);

  // Add dag_block_period in DB
  for (auto const blk_hash : schedule.dag_blks_order) {
    db_->addDagBlockPeriodToBatch(blk_hash, pbft_period, batch);
  }
  // Get dag blocks order
  blk_hash_t dag_block_hash = pbft_block.getPivotDagBlockHash();
  uint64_t current_period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(current_period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
<<<<<<< HEAD
=======
  // Add DAG blocks order and DAG blocks height in DB
  uint64_t max_dag_blocks_height = pbft_chain_->getDagBlockMaxHeight();
  for (auto const &dag_blk_hash : *dag_blocks_hash_order) {
    auto dag_block_height_ptr = db_->getDagBlockHeight(dag_blk_hash);
    if (dag_block_height_ptr) {
      LOG(log_er_) << "Duplicate DAG block " << dag_blk_hash
                   << " in DAG blocks height DB already";
      continue;
    }
    max_dag_blocks_height++;
    db_->addDagBlockOrderAndHeightToBatch(dag_blk_hash, max_dag_blocks_height,
                                          batch);
    LOG(log_dg_) << "Add dag block " << dag_blk_hash << " with height "
                 << max_dag_blocks_height << " in DB write batch";
  }
>>>>>>> logging improvements
  // Add cert votes in DB
  db_->addPbftCertVotesToBatch(pbft_block_hash, cert_votes, batch);
  LOG(log_dg_) << "Storing cert votes of pbft blk " << pbft_block_hash << "\n"
               << cert_votes;
  // Add PBFT block in DB
  db_->addPbftBlockToBatch(pbft_block, batch);
  // Update period_pbft_block in DB
  db_->addPbftBlockPeriodToBatch(pbft_period, pbft_block_hash, batch);
  // Update sortition account balance table DB
  updateSortitionAccountsDB_(batch);
  // TODO: Should remove PBFT chain head from DB
  // Update pbft chain
  // TODO: after remove PBFT chain head from DB, update pbft chain should after
  //  DB commit
  pbft_chain_->updatePbftChain(pbft_block_hash);
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain_->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain_->getJsonStr();
  db_->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  // Commit DB
  db_->commitWriteBatch(batch);
  LOG(log_dg_) << "DB write batch committed already";

  // Set DAG blocks period
  full_node->setDagBlockOrder(dag_block_hash, pbft_period);

  // Reset proposed PBFT block hash to False for next pbft block proposal
  proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);
  executed_pbft_block_ = true;

  // After DB commit, confirm in final chain(Ethereum)
  full_node->getFinalChain()->advance_confirm();
  // Remove executed transactions at Ethereum pending block. The Ethereum
  // pending block is same with latest block at Taraxa
  full_node->getPendingBlock()->advance(
      batch, new_eth_header.hash(),
      util::make_range_view(transactions_tmp_).map([](auto const &trx) {
        return trx.sha3();
      }));
  // Ethereum filter
  full_node->getFilterAPI()->note_block(new_eth_header.hash());
  full_node->getFilterAPI()->note_receipts(trx_receipts);
  // Update web server
  full_node->updateWsPbftBlockExecuted(pbft_block);
  if (auto ws_server = full_node->getWSServer()) {
    ws_server->newEthBlock(new_eth_header);
  }

  LOG(log_nf_) << full_node->getAddress() << " successful push pbft block "
               << pbft_block_hash << " in period " << pbft_period
               << " into chain! In round " << round_;
  return true;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  uint64_t last_pbft_period = pbft_chain_->getPbftChainSize();
  int64_t since_period;
  if (last_pbft_period < SKIP_PERIODS) {
    since_period = 0;
  } else {
    since_period = last_pbft_period - SKIP_PERIODS;
  }
  size_t active_players = 0;
  while (active_players == 0 && since_period >= 0) {
    active_players += std::count_if(
        sortition_account_balance_table.begin(),
        sortition_account_balance_table.end(),
        [since_period](std::pair<const taraxa::addr_t,
                                 taraxa::PbftSortitionAccount> const &account) {
          return (account.second.last_period_seen >= since_period);
        });
    if (active_players == 0) {
      LOG(log_wr_) << "Active players was found to be 0 since period "
                    << since_period
                    << ". Will go back one period continue to check. ";
      since_period--;
    }
  }
  auto full_node = node_.lock();
  addr_t account_address = full_node->getAddress();
  is_active_player_ =
      sortition_account_balance_table[account_address].last_period_seen >=
      since_period;
  if (active_players == 0) {
    // IF active_players count 0 then all players should be treated as active
    LOG(log_wr_) << "Active players was found to be 0! This should only "
                     "happen at initialization, when master boot node "
                     "distribute all of coins out to players. Will set active "
                     "player count to be sortition table size of "
                  << valid_sortition_accounts_size_ << ". Last period is "
                  << last_pbft_period;
    active_players = valid_sortition_accounts_size_;
    is_active_player_ = true;
  }

  // Update 2t+1 and threshold
  if (COMMITTEE_SIZE <= active_players) {
    TWO_T_PLUS_ONE = COMMITTEE_SIZE * 2 / 3 + 1;
    // round up
    sortition_threshold_ =
        (valid_sortition_accounts_size_ * COMMITTEE_SIZE - 1) / active_players +
        1;
  } else {
    TWO_T_PLUS_ONE = active_players * 2 / 3 + 1;
    sortition_threshold_ = valid_sortition_accounts_size_;
  }
  LOG(log_nf_) << "Committee size " << COMMITTEE_SIZE << ", active players "
               << active_players << " since period " << since_period
               << ", valid voting players " << valid_sortition_accounts_size_
               << ". Update 2t+1 " << TWO_T_PLUS_ONE << ", Threshold "
               << sortition_threshold_;
  ;
  active_nodes = active_players;  // TODO for test only
}

void PbftManager::updateTempSortitionAccountsTable_(
    uint64_t period, unordered_set<addr_t> const &dag_block_proposers,
    unordered_set<addr_t> const &trx_senders,
    unordered_map<addr_t, val_t> const &execution_touched_account_balances) {
  // Update temp PBFT sortition table for DAG block proposers who don't have
  // account balance changed (no transaction relative accounts)
  for (auto &addr : dag_block_proposers) {
    auto sortition_table_entry =
        std_find(sortition_account_balance_table_tmp, addr);
    if (sortition_table_entry) {
      auto &pbft_sortition_account = (*sortition_table_entry)->second;
      // fixme: weird lossy cast
      pbft_sortition_account.last_period_seen = static_cast<int64_t>(period);
      pbft_sortition_account.status = new_change;
    }
  }
  // Update PBFT sortition table for DAG block proposers who have account
  // balance changed
  for (auto &[addr, balance] : execution_touched_account_balances) {
    auto is_proposer = bool(std_find(dag_block_proposers, addr));
    auto is_sender = bool(std_find(trx_senders, addr));
    LOG(log_dg_) << "Externally owned account (is_sender: " << is_sender
                 << ") balance update: " << addr << " --> " << balance
                 << " in period " << period;
    auto enough_balance = balance >= VALID_SORTITION_COINS;
    auto sortition_table_entry =
        std_find(sortition_account_balance_table_tmp, addr);
    if (is_sender) {
      // Transaction sender
      if (sortition_table_entry) {
        auto &pbft_sortition_account = (*sortition_table_entry)->second;
        pbft_sortition_account.balance = balance;
        if (is_proposer) {
          // fixme: weird lossy cast
          pbft_sortition_account.last_period_seen =
              static_cast<int64_t>(period);
        }
        if (enough_balance) {
          pbft_sortition_account.status = new_change;
        } else {
          // After send coins doesn't have enough for sortition
          pbft_sortition_account.status = remove;
        }
      }
    } else {
      // Receiver
      if (enough_balance) {
        if (sortition_table_entry) {
          auto &pbft_sortition_account = (*sortition_table_entry)->second;
          pbft_sortition_account.balance = balance;
          if (is_proposer) {
            // TODO: weird lossy cast
            pbft_sortition_account.last_period_seen =
                static_cast<int64_t>(period);
          }
          pbft_sortition_account.status = new_change;
        } else {
          int64_t last_seen_period = -1;
          if (is_proposer) {
            // TODO: weird lossy cast
            last_seen_period = static_cast<int64_t>(period);
          }
          sortition_account_balance_table_tmp[addr] =
              PbftSortitionAccount(addr, balance, last_seen_period, new_change);
        }
      }
    }
  }
}

void PbftManager::updateSortitionAccountsTable_() {
  sortition_account_balance_table.clear();
  for (auto &account : sortition_account_balance_table_tmp) {
    sortition_account_balance_table[account.first] = account.second;
  }
  // update sortition accounts size
  valid_sortition_accounts_size_ = sortition_account_balance_table.size();
}

void PbftManager::updateSortitionAccountsDB_(DbStorage::BatchPtr const &batch) {
  for (auto &account : sortition_account_balance_table_tmp) {
    if (account.second.status == new_change) {
      db_->addSortitionAccountToBatch(account.first, account.second, batch);
      account.second.status = updated;
    } else if (account.second.status == remove) {
      // Erase both from DB and cache(table)
      db_->removeSortitionAccount(account.first);
      sortition_account_balance_table_tmp.erase(account.first);
    }
  }
  auto account_size =
      std::to_string(sortition_account_balance_table_tmp.size());
  db_->addSortitionAccountToBatch(std::string("sortition_accounts_size"),
                                  account_size, batch);
}

void PbftManager::countVotes_() {
  while (!monitor_stop_) {
    std::vector<Vote> votes = vote_mgr_->getAllVotes();

    size_t last_step_votes = 0;
    size_t current_step_votes = 0;
    for (auto const &v : votes) {
      if (step_ == 1) {
        if (v.getRound() == round_ - 1 && v.getStep() == last_step_) {
          last_step_votes++;
        } else if (v.getRound() == round_ && v.getStep() == step_) {
          current_step_votes++;
        }
      } else {
        if (v.getRound() == round_) {
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
        std::chrono::duration_cast<std::chrono::milliseconds>(
            last_step_duration)
            .count();

    auto current_step_duration = now - current_step_clock_initial_datetime_;
    auto elapsed_current_step_time_in_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            current_step_duration)
            .count();

    LOG(log_nf_test_) << "Round " << round_ << " step " << last_step_
                      << " time " << elapsed_last_step_time_in_ms << "(ms) has "
                      << last_step_votes << " votes";
    LOG(log_nf_test_) << "Round " << round_ << " step " << step_ << " time "
                      << elapsed_current_step_time_in_ms << "(ms) has "
                      << current_step_votes << " votes";
    thisThreadSleepForMilliSeconds(100);
  }
}

}  // namespace taraxa
