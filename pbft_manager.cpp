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
#include "util.hpp"
#include "util/eth.hpp"

namespace taraxa {

PbftManager::PbftManager(std::string const &genesis) : dag_genesis_(genesis) {}
PbftManager::PbftManager(std::vector<uint> const &params,
                         std::string const &genesis)
    // TODO: for debug, need remove later
    : LAMBDA_ms_MIN(params[0]),
      COMMITTEE_SIZE(params[1]),
      VALID_SORTITION_COINS(params[2]),
      DAG_BLOCKS_SIZE(params[3]),
      GHOST_PATH_MOVE_BACK(params[4]),
      RUN_COUNT_VOTES(params[5]),
      dag_genesis_(genesis) {}

void PbftManager::setFullNode(
    shared_ptr<taraxa::FullNode> full_node,
    shared_ptr<ReplayProtectionService> replay_protection_service) {
  node_ = full_node;
  vote_mgr_ = full_node->getVoteManager();
  pbft_chain_ = full_node->getPbftChain();
  capability_ = full_node->getNetwork()->getTaraxaCapability();
  replay_protection_service_ = replay_protection_service;
  db_ = full_node->getDB();
}

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  auto full_node = node_.lock();
  std::vector<std::string> ghost;
  full_node->getGhostPath(dag_genesis_, ghost);
  while (ghost.empty()) {
    LOG(log_deb_)
        << "GHOST is empty. DAG initialization has not done. Sleep 100ms";
    thisThreadSleepForMilliSeconds(100);
  }
  LOG(log_deb_) << "PBFT start at GHOST size " << ghost.size()
                << ", the last of DAG blocks is " << ghost.back();

  if (!db_->sortitionAccountInDb(std::string("sortition_accounts_size"))) {
    // New node
    // Initialize master boot node account balance
    addr_t master_boot_node_address = full_node->getMasterBootNodeAddress();
    std::pair<val_t, bool> master_boot_node_account_balance =
        full_node->getBalance(master_boot_node_address);
    if (!master_boot_node_account_balance.second) {
      LOG(log_err_) << "Failed initial master boot node account balance."
                    << " Master boot node balance is not exist.";
    } else if (master_boot_node_account_balance.first != TARAXA_COINS_DECIMAL) {
      LOG(log_inf_)
          << "Initial master boot node account balance. Current balance "
          << master_boot_node_account_balance.first;
    }
    PbftSortitionAccount master_boot_node(
        master_boot_node_address, master_boot_node_account_balance.first, 0,
        new_change);
    sortition_account_balance_table_tmp[master_boot_node_address] =
        master_boot_node;
    updateSortitionAccountsDB_();
    updateSortitionAccountsTable_();
  } else {
    // Full node join back
    if (!sortition_account_balance_table.empty()) {
      LOG(log_err_) << "PBFT sortition accounts table should be empty";
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

  // Reset round and step...
  pbft_round_last_ = 1;
  pbft_round_ = 1;
  pbft_step_ = 1;

  // Reset last sync request point...
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  daemon_ = std::make_unique<std::thread>([this]() { run(); });
  LOG(log_deb_) << "PBFT daemon initiated ...";
  if (RUN_COUNT_VOTES) {
    monitor_stop_ = false;
    monitor_votes_ = std::make_shared<std::thread>([this]() { countVotes_(); });
    LOG(log_inf_test_) << "PBFT monitor vote logs initiated";
  }
}

void PbftManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  if (RUN_COUNT_VOTES) {
    monitor_stop_ = true;
    monitor_votes_->join();
    LOG(log_inf_test_) << "PBFT monitor vote logs terminated";
  }
  daemon_->join();
  LOG(log_deb_) << "PBFT daemon terminated ...";
  replay_protection_service_ = nullptr;
}

/* When a node starts up it has to sync to the current phase (type of block
 * being generated) and step (within the block generation round)
 * Five step loop for block generation over three phases of blocks
 * User's credential, sigma_i_p for a round p is sig_i(R, p)
 * Leader l_i_p = min ( H(sig_j(R,p) ) over set of j in S_i where S_i is set of
 * users from which have received valid round p credentials
 */
void PbftManager::run() {
  LOG(log_inf_) << "PBFT executor running ...";

  // Initialize TWO_T_PLUS_ONE and sortition_threshold
  updateTwoTPlusOneAndThreshold_();

  // Initialize last block hash (PBFT genesis block in beginning)
  pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();

  auto round_clock_initial_datetime = std::chrono::system_clock::now();
  // <round, cert_voted_block_hash>
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_round;
  // <round, block_hash_added_into_chain>
  std::unordered_map<size_t, blk_hash_t> push_block_values_for_round;
  auto next_step_time_ms = 0;

  last_step_clock_initial_datetime_ = std::chrono::system_clock::now();
  current_step_clock_initial_datetime_ = std::chrono::system_clock::now();

  blk_hash_t own_starting_value_for_round = NULL_BLOCK_HASH;
  bool next_voted_soft_value = false;
  bool next_voted_null_block_hash = false;

  bool have_executed_this_round = false;
  bool should_have_cert_voted_in_this_round = false;

  LAMBDA_ms = LAMBDA_ms_MIN;

  next_voted_block_from_previous_round_ = std::make_pair(NULL_BLOCK_HASH, false);

  u_long STEP_4_DELAY = 2 * LAMBDA_ms;
  while (!stopped_) {
    auto now = std::chrono::system_clock::now();
    auto duration = now - round_clock_initial_datetime;
    auto elapsed_time_in_round_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    LOG(log_tra_) << "PBFT current round is " << pbft_round_;
    LOG(log_tra_) << "PBFT current step is " << pbft_step_;

    // push synced pbft blocks into chain
    pushSyncedPbftBlocksIntoChain_();
    // update pbft chain last block hash
    pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();

    // Get votes
    bool sync_peers_pbft_chain = false;
    std::vector<Vote> votes = vote_mgr_->getVotes(
        pbft_round_, valid_sortition_accounts_size_, sync_peers_pbft_chain);
    LOG(log_tra_) << "There are " << votes.size() << " total votes in round "
                  << pbft_round_;

    // Concern can malicious node trigger excessive syncing?
    if (sync_peers_pbft_chain && pbft_chain_->pbftSyncedQueueEmpty() &&
        capability_->syncing_ == false &&
        syncRequestedAlreadyThisStep_() == false) {
      LOG(log_sil_) << "Vote validation triggered pbft chain sync";
      syncPbftChainFromPeers_();
    }

    // CHECK IF WE HAVE RECEIVED 2t+1 CERT VOTES FOR A BLOCK IN OUR CURRENT
    // ROUND.  IF WE HAVE THEN WE EXECUTE THE BLOCK
    // ONLY CHECK IF HAVE *NOT* YET EXECUTED THIS ROUND...
    if ((pbft_step_ == 3 || pbft_step_ == 4) &&
        have_executed_this_round == false) {
      std::vector<Vote> cert_votes_for_round =
          getVotesOfTypeFromVotesForRoundAndStep_(
              cert_vote_type, votes, pbft_round_, 3,
              std::make_pair(NULL_BLOCK_HASH, false));
      // TODO: debug remove later
      LOG(log_tra_) << "Get cert votes for round " << pbft_round_ << " step "
                    << pbft_step_;
      std::pair<blk_hash_t, bool> cert_voted_block_hash =
          blockWithEnoughVotes_(cert_votes_for_round);
      if (cert_voted_block_hash.second) {
        LOG(log_deb_) << "PBFT block " << cert_voted_block_hash.first
                      << " has enough certed votes";

        // put pbft block into chain
        if (checkPbftBlockInUnverifiedQueue_(cert_voted_block_hash.first)) {
          if (pushCertVotedPbftBlockIntoChain_(cert_voted_block_hash.first)) {
            push_block_values_for_round[pbft_round_] =
                cert_voted_block_hash.first;
            have_executed_this_round = true;
            LOG(log_sil_) << "Write " << cert_votes_for_round.size()
                          << " votes ... in round " << pbft_round_;
            auto full_node = node_.lock();
            full_node->storeCertVotes(cert_voted_block_hash.first,
                                      cert_votes_for_round);

            // TODO: debug remove later
            LOG(log_deb_) << "The cert voted pbft block is "
                          << cert_voted_block_hash.first;
            duration = std::chrono::system_clock::now() - now;
            auto execute_trxs_in_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                    .count();
            LOG(log_deb_) << "Pushing PBFT block and Execution spent "
                          << execute_trxs_in_ms << " ms. in round "
                          << pbft_round_;
            continue;
          }
        } else {
          LOG(log_tra_) << "Still waiting to receive cert voted block "
                        << cert_voted_block_hash.first << " in round "
                        << pbft_round_;
        }
      }
    }
    // We skip step 4 due to having missed it while executing....
    if (have_executed_this_round == true &&
        elapsed_time_in_round_ms >
            4 * LAMBDA_ms + STEP_4_DELAY + 2 * POLLING_INTERVAL_ms &&
        pbft_step_ == 3) {
      LOG(log_deb_)
          << "Skipping step 4 due to execution, will go to step 5 in round "
          << pbft_round_;
      pbft_step_ = 5;
    }

    // Check if we are synced to the right step ...
    uint64_t consensus_pbft_round = roundDeterminedFromVotes_(votes);
    if (consensus_pbft_round > pbft_round_) {
      LOG(log_inf_) << "From votes determined round " << consensus_pbft_round;

      // p2p connection syncing should cover this situation, sync here for safe
      if (consensus_pbft_round > pbft_round_ + 1 &&
          capability_->syncing_ == false) {
        LOG(log_sil_) << "Quorum determined round " << consensus_pbft_round
                      << " > 1 + current round " << pbft_round_
                      << " local round, need to broadcast request for missing "
                         "certified blocks";

        // NOTE: Update this here before calling syncPbftChainFromPeers_
        //       to be sure this sync call won't be supressed for being too
        //       recent (ie. same round and step)
        pbft_round_ = consensus_pbft_round;
        pbft_step_ = 1;

        syncPbftChainFromPeers_();

        next_voted_block_from_previous_round_ = std::make_pair(NULL_BLOCK_HASH, false);

      } else {

        next_voted_block_from_previous_round_ = nextVotedBlockForRoundAndStep_(votes, pbft_round_);
      }

      // Update round and step...
      pbft_round_ = consensus_pbft_round;
      pbft_step_ = 1;  // Not strictly necessary since that is done inside next
                       // if statement

      // Update pbft chain last block hash at start of new round...
      pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();
    }
    if (pbft_round_ != pbft_round_last_) {
      round_clock_initial_datetime = now;

      have_executed_this_round = false;
      should_have_cert_voted_in_this_round = false;
      // reset starting value to NULL_BLOCK_HASH
      own_starting_value_for_round = NULL_BLOCK_HASH;
      // reset next voted value since start a new round
      next_voted_null_block_hash = false;
      next_voted_soft_value = false;
      if (executed_pbft_block_) {
        last_period_should_speak_ = pbft_chain_->getPbftChainPeriod();
        // Update sortition accounts table
        updateSortitionAccountsTable_();
        // reset sortition_threshold and TWO_T_PLUS_ONE
        updateTwoTPlusOneAndThreshold_();
        executed_pbft_block_ = false;
      }

      LAMBDA_ms = LAMBDA_ms_MIN;

      // NOTE: This also sets pbft_step back to 1
      last_step_ = pbft_step_;
      pbft_step_ = 1;
      last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
      current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
      pbft_round_last_ = pbft_round_;
      LOG(log_deb_) << "Advancing clock to pbft round " << pbft_round_
                    << ", step 1, and resetting clock.";
      continue;
    }

    if (pbft_step_ == 1) {
      // Value Proposal
      if (shouldSpeak(propose_vote_type, pbft_round_, pbft_step_)) {
        if (pbft_round_ == 1) {
          LOG(log_deb_) << "Proposing value of NULL_BLOCK_HASH "
                        << NULL_BLOCK_HASH << " for round 1 by protocol";
          placeVote_(own_starting_value_for_round, propose_vote_type,
                     pbft_round_, pbft_step_);
        } else if (push_block_values_for_round.count(pbft_round_ - 1) ||
                   (pbft_round_ >= 2 && nullBlockNextVotedForRoundAndStep_(
                                            votes, pbft_round_ - 1))) {
          // PBFT block only be proposed once in one period
          if (!proposed_block_hash_.second ||
              proposed_block_hash_.first == NULL_BLOCK_HASH) {
            // Propose value...
            proposed_block_hash_ = proposeMyPbftBlock_();
          }
          if (proposed_block_hash_.second) {
            own_starting_value_for_round = proposed_block_hash_.first;
            placeVote_(proposed_block_hash_.first, propose_vote_type,
                       pbft_round_, pbft_step_);
          }
        } else if (pbft_round_ >= 2) {
          if (next_voted_block_from_previous_round_.second &&
              next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
            LOG(log_deb_) << "Proposing next voted block "
                          << next_voted_block_from_previous_round_.first
                          << " from previous round";
            placeVote_(next_voted_block_from_previous_round_.first,
                       propose_vote_type, pbft_round_, pbft_step_);
          }
        }
      }
      next_step_time_ms = 2 * LAMBDA_ms;
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;
      last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
      current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
      last_step_ = pbft_step_;
      pbft_step_ += 1;

    } else if (pbft_step_ == 2) {
      // The Filtering Step
      if (shouldSpeak(soft_vote_type, pbft_round_, pbft_step_)) {
        if (pbft_round_ == 1 ||
            (pbft_round_ >= 2 &&
             push_block_values_for_round.count(pbft_round_ - 1)) ||
            (pbft_round_ >= 2 &&
             nullBlockNextVotedForRoundAndStep_(votes, pbft_round_ - 1))) {
          // Identity leader
          std::pair<blk_hash_t, bool> leader_block =
              identifyLeaderBlock_(votes);
          if (leader_block.second) {
            LOG(log_deb_) << "Identify leader block " << leader_block.first
                          << " for round " << pbft_round_
                          << " and soft vote the value";
            if (own_starting_value_for_round == NULL_BLOCK_HASH) {
              own_starting_value_for_round = leader_block.first;
            }
            placeVote_(leader_block.first, soft_vote_type, pbft_round_,
                       pbft_step_);
          }
        } else if (pbft_round_ >= 2) {
          if (next_voted_block_from_previous_round_.second &&
              next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
            LOG(log_deb_) << "Soft voting "
                          << next_voted_block_from_previous_round_.first
                          << " from previous round";
            placeVote_(next_voted_block_from_previous_round_.first,
                       soft_vote_type, pbft_round_, pbft_step_);
          }
        }
      }

      next_step_time_ms = 2 * LAMBDA_ms;
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;
      last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
      current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
      last_step_ = pbft_step_;
      pbft_step_ += 1;

    } else if (pbft_step_ == 3) {
      // The Certifying Step
      if (elapsed_time_in_round_ms < 2 * LAMBDA_ms) {
        // Should not happen, add log here for safety checking
        LOG(log_err_) << "PBFT Reached step 3 too quickly after only "
                      << elapsed_time_in_round_ms << " (ms) in round "
                      << pbft_round_;
      }

      bool should_go_to_step_four = false;

      if (elapsed_time_in_round_ms >
          4 * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms) {
        LOG(log_deb_) << "Step 3 expired, will go to step 4 in round "
                      << pbft_round_;
        should_go_to_step_four = true;
      } else if (should_have_cert_voted_in_this_round == false) {
        LOG(log_tra_) << "In step 3";
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_round =
            softVotedBlockForRound_(votes, pbft_round_);

        LOG(log_tra_) << "Finished softVotedBlockForRound_";

        if (soft_voted_block_for_this_round.second &&
            soft_voted_block_for_this_round.first != NULL_BLOCK_HASH &&
            comparePbftBlockScheduleWithDAGblocks_(
                soft_voted_block_for_this_round.first)) {
          LOG(log_tra_) << "Finished comparePbftBlockScheduleWithDAGblocks_";

          // NOTE: If we have already executed this round
          //       then block won't be found in unverified queue...
          bool executed_soft_voted_block_for_this_round = false;
          if (have_executed_this_round == true) {
            LOG(log_tra_)
                << "Have already executed before certifying in step 3 in round "
                << pbft_round_;

            if (pbft_chain_->getLastPbftBlockHash() ==
                soft_voted_block_for_this_round.first) {
              LOG(log_tra_) << "Having executed, last block in chain is the "
                               "soft voted block in round "
                            << pbft_round_;
              executed_soft_voted_block_for_this_round = true;
            }
          }

          bool unverified_soft_vote_block_for_this_round_is_valid = false;
          if (executed_soft_voted_block_for_this_round == false) {
            if (checkPbftBlockInUnverifiedQueue_(
                    soft_voted_block_for_this_round.first)) {
              if (checkPbftBlockValid_(soft_voted_block_for_this_round.first)) {
                LOG(log_tra_) << "checkPbftBlockValid_ returned true";
                unverified_soft_vote_block_for_this_round_is_valid = true;
              } else {
                // Get partition, need send request to get missing pbft blocks
                // from peers
                LOG(log_sil_)
                    << "Soft voted block for this round appears to be invalid, "
                       "we must be out of sync with pbft chain";

                if (capability_->syncing_ == false) {
                  syncPbftChainFromPeers_();
                }
              }
            } else {
              LOG(log_tra_) << "Still waiting to receive the soft voted block "
                            << soft_voted_block_for_this_round.first
                            << " in step 3 in round " << pbft_round_;
            }
          }

          if (executed_soft_voted_block_for_this_round == true ||
              unverified_soft_vote_block_for_this_round_is_valid == true) {
            cert_voted_values_for_round[pbft_round_] =
                soft_voted_block_for_this_round.first;

            // NEED TO KEEP POLLING TO SEE IF WE HAVE 2t+1 cert votes...
            // Here we would cert vote if we can speak....
            should_have_cert_voted_in_this_round = true;
            if (shouldSpeak(cert_vote_type, pbft_round_, pbft_step_)) {
              LOG(log_deb_)
                  << "Cert voting " << soft_voted_block_for_this_round.first
                  << " for round " << pbft_round_;
              // generate cert vote
              placeVote_(soft_voted_block_for_this_round.first, cert_vote_type,
                         pbft_round_, pbft_step_);
            }
          }
        }
      }

      if (should_go_to_step_four) {
        LOG(log_deb_) << "will go to step 4";
        next_step_time_ms = 4 * LAMBDA_ms + STEP_4_DELAY;
        last_step_clock_initial_datetime_ =
            current_step_clock_initial_datetime_;
        current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
        last_step_ = pbft_step_;
        pbft_step_ += 1;
      } else {
        next_step_time_ms += POLLING_INTERVAL_ms;
      }
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;

    } else if (pbft_step_ == 4) {
      if (shouldSpeak(next_vote_type, pbft_round_, pbft_step_)) {
        if (cert_voted_values_for_round.find(pbft_round_) !=
            cert_voted_values_for_round.end()) {
          LOG(log_deb_) << "Next voting value "
                        << cert_voted_values_for_round[pbft_round_]
                        << " for round " << pbft_round_;
          placeVote_(cert_voted_values_for_round[pbft_round_], next_vote_type,
                     pbft_round_, pbft_step_);
        } else if (pbft_round_ >= 2 &&
                   nullBlockNextVotedForRoundAndStep_(votes, pbft_round_ - 1)) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
        } else {
          LOG(log_deb_) << "Next voting nodes own starting value "
                        << own_starting_value_for_round << " for round "
                        << pbft_round_;
          placeVote_(own_starting_value_for_round, next_vote_type, pbft_round_,
                     pbft_step_);
        }
      }
      last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
      current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
      last_step_ = pbft_step_;
      pbft_step_ += 1;
      next_step_time_ms = 4 * LAMBDA_ms + STEP_4_DELAY;
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;

    } else if (pbft_step_ == 5) {
      if (elapsed_time_in_round_ms >
          6 * LAMBDA_ms + STEP_4_DELAY + 2 * POLLING_INTERVAL_ms) {
        // Should not happen, add log here for safety checking
        if (have_executed_this_round == true) {
          LOG(log_deb_) << "PBFT Reached round " << pbft_round_
                        << " step 5 late due to execution";
        } else {
          LOG(log_deb_) << "PBFT Reached round " << pbft_round_
                        << " step 5 late without executing";
        }
        pbft_step_ = 7;
        continue;
      }
      if (shouldSpeak(next_vote_type, pbft_round_, pbft_step_)) {
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_round =
            softVotedBlockForRound_(votes, pbft_round_);
        if (!next_voted_soft_value && soft_voted_block_for_this_round.second &&
            soft_voted_block_for_this_round.first != NULL_BLOCK_HASH &&
            comparePbftBlockScheduleWithDAGblocks_(
                soft_voted_block_for_this_round.first)) {
          LOG(log_deb_) << "Next voting "
                        << soft_voted_block_for_this_round.first
                        << " for round " << pbft_round_;
          placeVote_(soft_voted_block_for_this_round.first, next_vote_type,
                     pbft_round_, pbft_step_);
          next_voted_soft_value = true;
        }
        if (!next_voted_null_block_hash && pbft_round_ >= 2 &&
            nullBlockNextVotedForRoundAndStep_(votes, pbft_round_ - 1) &&
            (cert_voted_values_for_round.find(pbft_round_) ==
             cert_voted_values_for_round.end())) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_
                        << " step " << pbft_step_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
          next_voted_null_block_hash = true;
        }
      }

      if (elapsed_time_in_round_ms >
          6 * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms) {
        next_step_time_ms = 6 * LAMBDA_ms + STEP_4_DELAY;
        last_step_clock_initial_datetime_ =
            current_step_clock_initial_datetime_;
        current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
        last_step_ = pbft_step_;
        pbft_step_ += 1;
        next_voted_soft_value = false;
        next_voted_null_block_hash = false;
      } else {
        next_step_time_ms += POLLING_INTERVAL_ms;
      }
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;

    } else if (pbft_step_ % 2 == 0) {
      // Even number steps 6, 8, 10... < MAX_STEPS are a repeat of step 4...
      if (shouldSpeak(next_vote_type, pbft_round_, pbft_step_)) {
        if (cert_voted_values_for_round.find(pbft_round_) !=
            cert_voted_values_for_round.end()) {
          LOG(log_deb_) << "Next voting value "
                        << cert_voted_values_for_round[pbft_round_]
                        << " for round " << pbft_round_;
          placeVote_(cert_voted_values_for_round[pbft_round_], next_vote_type,
                     pbft_round_, pbft_step_);
        } else if (pbft_round_ >= 2 &&
                   nullBlockNextVotedForRoundAndStep_(votes, pbft_round_ - 1)) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
        } else {
          LOG(log_deb_) << "Next voting nodes own starting value for round "
                        << pbft_round_;
          placeVote_(own_starting_value_for_round, next_vote_type, pbft_round_,
                     pbft_step_);
        }
      }
      last_step_clock_initial_datetime_ = current_step_clock_initial_datetime_;
      current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
      last_step_ = pbft_step_;
      pbft_step_ += 1;

      if (pbft_step_ > MAX_STEPS) {
        LAMBDA_ms *= 2;
        LOG(log_inf_) << "Surpassed max steps, relaxing lambda to " << LAMBDA_ms
                      << " ms in round " << pbft_round_ << ", step "
                      << pbft_step_;
      }

    } else {
      // Odd number steps 7, 9, 11... < MAX_STEPS are a repeat of step 5...
      if (elapsed_time_in_round_ms > (pbft_step_ + 1) * LAMBDA_ms +
                                         STEP_4_DELAY +
                                         2 * POLLING_INTERVAL_ms) {
        // Should not happen, add log here for safety checking
        if (have_executed_this_round == true) {
          LOG(log_deb_) << "PBFT Reached round " << pbft_round_ << " step "
                        << pbft_step_ << " late due to execution";
        } else {
          LOG(log_deb_) << "PBFT Reached round " << pbft_round_ << " step "
                        << pbft_step_ << " late without executing";
        }
        pbft_step_ += 2;
        continue;
      }

      if (shouldSpeak(next_vote_type, pbft_round_, pbft_step_)) {
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_round =
            softVotedBlockForRound_(votes, pbft_round_);
        if (!next_voted_soft_value && soft_voted_block_for_this_round.second &&
            soft_voted_block_for_this_round.first != NULL_BLOCK_HASH &&
            comparePbftBlockScheduleWithDAGblocks_(
                soft_voted_block_for_this_round.first)) {
          LOG(log_deb_) << "Next voting "
                        << soft_voted_block_for_this_round.first
                        << " for round " << pbft_round_;
          placeVote_(soft_voted_block_for_this_round.first, next_vote_type,
                     pbft_round_, pbft_step_);
          next_voted_soft_value = true;
        }
        if (!next_voted_null_block_hash && pbft_round_ >= 2 &&
            nullBlockNextVotedForRoundAndStep_(votes, pbft_round_ - 1) &&
            (cert_voted_values_for_round.find(pbft_round_) ==
             cert_voted_values_for_round.end())) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
          next_voted_null_block_hash = true;
        }

        /*
        if (!next_voted_soft_value && !next_voted_null_block_hash &&
            pbft_step_ >= MAX_STEPS) {
          LOG(log_deb_) << "Next voting NULL BLOCK HAVING REACHED MAX STEPS "
                           "for round "
                        << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
          next_voted_null_block_hash = true;
        }*/
      }

      if (pbft_step_ > MAX_STEPS && capability_->syncing_ == false &&
          syncRequestedAlreadyThisStep_() == false) {
        LOG(log_war_) << "Suspect pbft chain behind, inaccurate 2t+1, need "
                         "to broadcast request for missing blocks";
        syncPbftChainFromPeers_();
      }

      // if (pbft_step_ >= MAX_STEPS) {
      /*
      pbft_round_ += 1;
      LOG(log_deb_) << "Having next voted, advancing clock to pbft round "
                    << pbft_round_ << ", step 1, and resetting clock.";
      // reset starting value to NULL_BLOCK_HASH
      own_starting_value_for_round = NULL_BLOCK_HASH;
      // I added this as a way of seeing if we were even getting votes during
      // testnet -Justin
      LOG(log_deb_) << "There are " << votes.size() << " votes since round "
                    << pbft_round_ - 2;
      // NOTE: This also sets pbft_step back to 1
      last_step_clock_initial_datetime_ =
          current_step_clock_initial_datetime_;
      current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
      last_step_ = pbft_step_;
      pbft_step_ = 1;
      next_voted_soft_value = false;
      next_voted_null_block_hash = false;
      round_clock_initial_datetime = std::chrono::system_clock::now();
      */
      //  continue;
      //} else
      if (elapsed_time_in_round_ms >
          (pbft_step_ + 1) * LAMBDA_ms + STEP_4_DELAY - POLLING_INTERVAL_ms) {
        next_step_time_ms = (pbft_step_ + 1) * LAMBDA_ms + STEP_4_DELAY;
        last_step_clock_initial_datetime_ =
            current_step_clock_initial_datetime_;
        current_step_clock_initial_datetime_ = std::chrono::system_clock::now();
        last_step_ = pbft_step_;
        pbft_step_ += 1;
        next_voted_soft_value = false;
        next_voted_null_block_hash = false;
      } else {
        next_step_time_ms += POLLING_INTERVAL_ms;
      }
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;
    }

    now = std::chrono::system_clock::now();
    duration = now - round_clock_initial_datetime;
    elapsed_time_in_round_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    auto time_to_sleep_for_ms = next_step_time_ms - elapsed_time_in_round_ms;
    if (time_to_sleep_for_ms > 0) {
      LOG(log_tra_) << "Time to sleep(ms): " << time_to_sleep_for_ms
                    << " in round " << pbft_round_ << ", step " << pbft_step_;
      thisThreadSleepForMilliSeconds(time_to_sleep_for_ms);
    }
  }
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

std::string PbftManager::getScheduleBlockByPeriod(uint64_t period) {
  auto value = db_->getPeriodScheduleBlock(period);
  if (value) {
    blk_hash_t pbft_block_hash = *value;
    return pbft_chain_->getPbftBlockInChain(pbft_block_hash).getJsonStr();
  }
  return "";
}

bool PbftManager::shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable";
    return false;
  }
  addr_t account_address = full_node->getAddress();
  if (sortition_account_balance_table.find(account_address) ==
      sortition_account_balance_table.end()) {
    LOG(log_tra_) << "Cannot find account " << account_address
                  << " in sortition table. Don't have enough coins to vote";
    return false;
  }
  // only active players are able to vote
  if (!is_active_player_) {
    LOG(log_tra_)
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
    LOG(log_tra_) << "Don't get sortition";
    return false;
  }
  return true;
}

/* There is a quorum of next-votes and set determine that round p should be the
 * current round...
 */
uint64_t PbftManager::roundDeterminedFromVotes_(std::vector<Vote> votes) {
  // <<vote_round, vote_step>, count>, <round, step> store in reverse order
  std::map<std::pair<uint64_t, size_t>, size_t,
           std::greater<std::pair<uint64_t, size_t>>>
      next_votes_tally_by_round_step;

  votes = vote_mgr_->getAllVotes();
  for (Vote &v : votes) {
    if (v.getType() != next_vote_type) {
      continue;
    }

    std::pair<uint64_t, size_t> round_step =
        std::make_pair(v.getRound(), v.getStep());
    if (round_step.first >= pbft_round_) {
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
              next_vote_type, votes, rs_votes.first.first,
              rs_votes.first.second, std::make_pair(NULL_BLOCK_HASH, false));
      if (blockWithEnoughVotes_(next_votes_for_round_step).second) {
        LOG(log_deb_) << "Found sufficient next votes in round "
                      << rs_votes.first.first << ", step "
                      << rs_votes.first.second;
        return rs_votes.first.first + 1;
      }
    }
  }
  return pbft_round_;
}

// Assumption is that all votes are in the same round and of same type...
std::pair<blk_hash_t, bool> PbftManager::blockWithEnoughVotes_(
    std::vector<Vote> &votes) const {
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
        LOG(log_err_) << "Vote types and rounds were not internally"
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
        LOG(log_deb_) << "find block hash " << blockhash_pair.first
                      << " vote type " << vote_type << " in round "
                      << vote_round << " has " << blockhash_pair.second
                      << " votes";
        return std::make_pair(blockhash_pair.first, true);
      } else {
        LOG(log_tra_) << "Don't have enough votes. block hash "
                      << blockhash_pair.first << " vote type " << vote_type
                      << " in round " << vote_round << " has "
                      << blockhash_pair.second << " votes"
                      << " (2TP1 = " << TWO_T_PLUS_ONE << ")";
      }
    }
  }

  return std::make_pair(NULL_BLOCK_HASH, false);
}

bool PbftManager::nullBlockNextVotedForRoundAndStep_(std::vector<Vote> &votes,
                                                     uint64_t round) {
  blk_hash_t blockhash = NULL_BLOCK_HASH;
  std::pair<blk_hash_t, bool> blockhash_pair = std::make_pair(blockhash, true);
  std::map<size_t, std::vector<Vote>, std::greater<size_t>>
      null_block_votes_in_round_by_step =
          getVotesOfTypeFromVotesForRoundByStep_(next_vote_type, votes, round,
                                                 blockhash_pair);

  for (auto const &sv : null_block_votes_in_round_by_step) {
    if (sv.second.size() >= TWO_T_PLUS_ONE) {
      return true;
    }
  }
  return false;
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

  for (Vote &v : votes) {
    if (v.getType() == vote_type && v.getRound() == round &&
        v.getStep() == step &&
        (blockhash.second == false || blockhash.first == v.getBlockHash())) {
      votes_of_requested_type.emplace_back(v);
    }
  }

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
  LOG(log_deb_) << "vote block hash: " << blockhash
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
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  LOG(log_deb_) << "Into propose PBFT block";
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
  LOG(log_deb_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at lease include the last period dag anchor
  // block
  if (ghost.empty()) {
    LOG(log_deb_) << "GHOST is empty. No new DAG blocks generated, PBFT "
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
    LOG(log_deb_) << "No new DAG blocks generated. DAG only has genesis "
                  << dag_block_hash << " PBFT propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // compare with last dag block hash. If they are same, which means no new
  // dag blocks generated since last round. In that case PBFT proposer should
  // propose NULL BLOCK HASH as their value and not produce a new block. In
  // practice this should never happen
  if (dag_block_hash.toString() == last_period_dag_anchor_block_hash) {
    LOG(log_deb_)
        << "Last period DAG anchor block hash " << dag_block_hash
        << " No new DAG blocks generated, PBFT propose NULL_BLOCK_HASH";
    LOG(log_deb_) << "Ghost: " << ghost;
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // get dag blocks hash order
  uint64_t pbft_chain_period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(pbft_chain_period, dag_blocks_hash_order) =
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
      if (!replay_protection_service_->hasBeenExecuted(*trx)) {
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
  //    LOG(log_err_) << "Transaction overlap table nullptr, cannot create mock
  //    "
  //                  << "transactions schedule";
  //    return std::make_pair(NULL_BLOCK_HASH, false);
  //  }
  //  if (trx_overlap_table->empty()) {
  //    LOG(log_deb_) << "Transaction overlap table is empty, no DAG block needs
  //    "
  //                  << " generate mock trx schedule";
  //    return std::make_pair(NULL_BLOCK_HASH, false);
  //  }
  //  // TODO: generate fake transaction schedule for now, will pass
  //  //  trx_overlap_table to VM
  //  std::vector<std::vector<uint>> dag_blocks_trx_modes =
  //      full_node->createMockTrxSchedule(trx_overlap_table);

  TrxSchedule schedule(*dag_blocks_hash_order, dag_blocks_trxs_mode);
  uint64_t propose_pbft_period = pbft_chain_->getPbftChainPeriod() + 1;
  uint64_t pbft_block_height = pbft_chain_->getPbftChainSize() + 1;
  uint64_t timestamp = std::time(nullptr);
  addr_t beneficiary = full_node->getAddress();

  // generate generate pbft block
  PbftBlock pbft_block(last_pbft_block_hash, dag_block_hash, schedule,
                       propose_pbft_period, pbft_block_height, timestamp,
                       beneficiary);
  // sign the pbft block
  std::string pbft_block_str = pbft_block.getJsonStr(false);
  sig_t signature = full_node->signMessage(pbft_block_str);
  pbft_block.setSignature(signature);
  pbft_block.setBlockHash();
  // push pbft block
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
  // broadcast pbft block
  std::shared_ptr<Network> network = full_node->getNetwork();
  network->onNewPbftBlock(pbft_block);

  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  LOG(log_deb_) << full_node->getAddress() << " propose PBFT block succussful! "
                << " in round: " << pbft_round_ << " in step: " << pbft_step_
                << " PBFT block: " << pbft_block;
  return std::make_pair(pbft_block_hash, true);
}

std::pair<blk_hash_t, bool> PbftManager::identifyLeaderBlock_(
    std::vector<Vote> &votes) {
  LOG(log_deb_) << "Into identify leader block, in round " << pbft_round_;
  // each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<blk_hash_t, blk_hash_t>> leader_candidates;
  for (auto const &v : votes) {
    if (v.getRound() == pbft_round_ && v.getType() == propose_vote_type) {
      leader_candidates.emplace_back(
          std::make_pair(dev::sha3(v.getVoteSignature()), v.getBlockHash()));
    }
  }
  if (leader_candidates.empty()) {
    // no eligible leader
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  std::pair<blk_hash_t, blk_hash_t> leader = leader_candidates[0];
  for (auto const &candinate : leader_candidates) {
    if (candinate.first < leader.first) {
      leader = candinate;
    }
  }
  return std::make_pair(leader.second, true);
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(
    taraxa::blk_hash_t const &cert_voted_block_hash) {
  if (!checkPbftBlockValid_(cert_voted_block_hash)) {
    // Get partition, need send request to get missing pbft blocks from peers
    LOG(log_sil_) << "Cert voted block " << cert_voted_block_hash
                  << " is invalid, we must be out of sync with pbft chain";
    if (capability_->syncing_ == false) {
      syncPbftChainFromPeers_();
    }
    return false;
  }
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getUnverifiedPbftBlock(cert_voted_block_hash);
  if (!pbft_block.second) {
    LOG(log_err_) << "Can not find the cert vote block hash "
                  << cert_voted_block_hash << " in pbft queue";
    return false;
  }
  if (!pushPbftBlockIntoChain_(pbft_block.first)) {
    // Push PBFT block from unverified blocks table
    return false;
  }
  // cleanup PBFT unverified blocks table
  pbft_chain_->cleanupUnverifiedPbftBlocks(pbft_block.first);
  return true;
}

bool PbftManager::checkPbftBlockInUnverifiedQueue_(
    blk_hash_t const &block_hash) const {
  std::pair<PbftBlock, bool> cert_voted_block =
      pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!cert_voted_block.second) {
    LOG(log_tra_) << "Cannot find the unverified pbft block, block hash "
                  << block_hash;
  }
  return cert_voted_block.second;
}

bool PbftManager::checkPbftBlockValid_(blk_hash_t const &block_hash) const {
  std::pair<PbftBlock, bool> cert_voted_block =
      pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!cert_voted_block.second) {
    LOG(log_err_) << "Cannot find the unverified pbft block, block hash "
                  << block_hash;
    return false;
  }
  blk_hash_t prev_block_hash = cert_voted_block.first.getPrevBlockHash();
  if (pbft_chain_->getLastPbftBlockHash() != prev_block_hash) {
    LOG(log_inf_) << "Pbft chain last block hash "
                  << pbft_chain_->getLastPbftBlockHash()
                  << " Invalid pbft prev block hash " << prev_block_hash;
    return false;
  }
  return true;
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return pbft_round_ == pbft_round_last_requested_sync_ &&
         pbft_step_ == pbft_step_last_requested_sync_;
}

void PbftManager::syncPbftChainFromPeers_() {
  if (!pbft_chain_->pbftSyncedQueueEmpty()) {
    LOG(log_deb_) << "DAG has not synced yet. PBFT chain skips syncing";
    return;
  }

  if (capability_->syncing_ == false) {
    if (syncRequestedAlreadyThisStep_() == false) {
      LOG(log_sil_) << "Restarting pbft sync."
                    << " In round " << pbft_round_ << ", in step " << pbft_step_
                    << " Send request to ask missing pbft blocks in chain";
      capability_->restartSyncingPbft();
      pbft_round_last_requested_sync_ = pbft_round_;
      pbft_step_last_requested_sync_ = pbft_step_;
    }
  }
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(
    blk_hash_t const &pbft_block_hash) {
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getUnverifiedPbftBlock(pbft_block_hash);
  if (!pbft_block.second) {
    LOG(log_deb_) << "Have not got the PBFT block yet. block hash: "
                  << pbft_block_hash;
    return false;
  }

  return comparePbftBlockScheduleWithDAGblocks_(pbft_block.first);
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(
    PbftBlock const &pbft_block) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return false;
  }
  // get DAG blocks order
  blk_hash_t dag_block_hash = pbft_block.getPivotDagBlockHash();
  uint64_t pbft_chain_period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(pbft_chain_period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // compare DAG blocks hash in PBFT schedule with DAG blocks
  vec_blk_t dag_blocks_hash_in_schedule =
      pbft_block.getSchedule().dag_blks_order;
  if (dag_blocks_hash_in_schedule.size() == dag_blocks_hash_order->size()) {
    for (auto i = 0; i < dag_blocks_hash_in_schedule.size(); i++) {
      if (dag_blocks_hash_in_schedule[i] != (*dag_blocks_hash_order)[i]) {
        LOG(log_inf_) << "DAG blocks have not sync yet. In period: "
                      << pbft_chain_->getPbftChainPeriod()
                      << ", DAG block hash " << dag_blocks_hash_in_schedule[i]
                      << " in PBFT schedule is different with DAG block hash "
                      << (*dag_blocks_hash_order)[i];
        return false;
      }
    }
  } else {
    LOG(log_inf_) << "DAG blocks have not sync yet. In period: "
                  << pbft_chain_->getPbftChainPeriod()
                  << " PBFT block schedule DAG blocks size: "
                  << dag_blocks_hash_in_schedule.size()
                  << ", DAG blocks size: " << dag_blocks_hash_order->size();
    // For debug
    if (dag_blocks_hash_order->size() > dag_blocks_hash_in_schedule.size()) {
      LOG(log_err_) << "Print DAG blocks order:";
      for (auto const &block : *dag_blocks_hash_order) {
        LOG(log_err_) << "block: " << block;
      }
      LOG(log_err_) << "Print DAG blocks in PBFT schedule order:";
      for (auto const &block : dag_blocks_hash_in_schedule) {
        LOG(log_err_) << "block: " << block;
      }
      std::string filename = "unmatched_pbft_schedule_order_in_period_" +
                             std::to_string(pbft_chain_->getPbftChainPeriod());
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
  //      LOG(log_err_) << "In DAG block hash: " << (*dag_blocks_hash_order)[i]
  //                    << " has " << dag_block_trxs.size()
  //                    << " transactions. But the DAG block in PBFT CS block "
  //                    << "only has " << trx_modes[i].size() << "
  //                    transactions.";
  //      return false;
  //    }
  //  }

  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain_() {
  bool queue_was_full = false;
  size_t pbft_synced_queue_size;
  auto full_node = node_.lock();
  while (!pbft_chain_->pbftSyncedQueueEmpty()) {
    queue_was_full = true;
    PbftBlockCert pbft_block_and_votes = pbft_chain_->pbftSyncedQueueFront();
    LOG(log_deb_) << "Pick pbft block "
                  << pbft_block_and_votes.pbft_blk.getBlockHash()
                  << " from synced queue in round " << pbft_round_;
    if (pbft_chain_->findPbftBlockInChain(
            pbft_block_and_votes.pbft_blk.getBlockHash())) {
      // pushed already from PBFT unverified queue, remove and skip it
      pbft_chain_->pbftSyncedQueuePopFront();

      pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
        LOG(log_deb_) << "PBFT block "
                      << pbft_block_and_votes.pbft_blk.getBlockHash()
                      << " already present in chain.";
        LOG(log_deb_) << "PBFT synced queue still contains "
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
      LOG(log_err_)
          << "Synced PBFT block "
          << pbft_block_and_votes.pbft_blk.getBlockHash()
          << " doesn't have enough valid cert votes. Clear synced PBFT blocks!";
      pbft_chain_->clearSyncedPbftBlocks();
      break;
    }
    if (!pushPbftBlockIntoChain_(pbft_block_and_votes.pbft_blk)) {
      // PBFT chain syncing faster than DAG syncing, wait!
      pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
        LOG(log_deb_) << "PBFT chain unable to push synced block "
                      << pbft_block_and_votes.pbft_blk.getBlockHash();
        LOG(log_deb_) << "PBFT synced queue still contains "
                      << pbft_synced_queue_size
                      << " synced blocks that could not be pushed.";
      }
      pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;
      break;
    }
    pbft_chain_->pbftSyncedQueuePopFront();
    // Store cert votes in DB
    full_node->storeCertVotes(pbft_block_and_votes.pbft_blk.getBlockHash(),
                              pbft_block_and_votes.cert_votes);
    if (executed_pbft_block_) {
      last_period_should_speak_ = pbft_chain_->getPbftChainPeriod();
      // Update sortition accounts table
      updateSortitionAccountsTable_();
      // update sortition_threshold and TWO_T_PLUS_ONE
      updateTwoTPlusOneAndThreshold_();
      executed_pbft_block_ = false;
    }
    pbft_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
    if (pbft_last_observed_synced_queue_size_ != pbft_synced_queue_size) {
      LOG(log_deb_) << "PBFT synced queue still contains "
                    << pbft_synced_queue_size
                    << " synced blocks that could not be pushed.";
    }
    pbft_last_observed_synced_queue_size_ = pbft_synced_queue_size;
  }

  /*
  if (queue_was_full == true && pbft_chain_->pbftSyncedQueueEmpty()) {
    LOG(log_inf_) << "PBFT synced queue is newly empty.  Will check if "
                     "need to sync in round "
                  << pbft_round_;
    syncPbftChainFromPeers_();
  }
  */
}

bool PbftManager::pushPbftBlockIntoChain_(PbftBlock const &pbft_block) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return false;
  }
  if (comparePbftBlockScheduleWithDAGblocks_(pbft_block)) {
    if (pbft_chain_->pushPbftBlock(pbft_block)) {
      LOG(log_inf_) << "Successful push pbft block "
                    << pbft_block.getBlockHash() << " into chain! in round "
                    << pbft_round_;
      // reset proposed PBFT block hash to False for next pbft block proposal
      proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);
      // get dag blocks order
      blk_hash_t last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
      PbftBlock last_pbft_block =
          pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
      blk_hash_t dag_block_hash = last_pbft_block.getPivotDagBlockHash();
      uint64_t current_period;
      std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
      std::tie(current_period, dag_blocks_hash_order) =
          full_node->getDagBlockOrder(dag_block_hash);
      // update DAG blocks order and DAG blocks order/height DB
      for (auto const &dag_blk_hash : *dag_blocks_hash_order) {
        auto block_number = pbft_chain_->pushDagBlockHash(dag_blk_hash);
      }
      // set DAG blocks period
      uint64_t current_pbft_chain_period = last_pbft_block.getPeriod();
      uint dag_ordered_blocks_size = full_node->setDagBlockOrder(
          dag_block_hash, current_pbft_chain_period);
      // Finalize PBFT block
      LOG(log_deb_) << full_node->getAddress()
                    << " Finalize PBFT block in period "
                    << current_pbft_chain_period << " round " << pbft_round_
                    << " step " << pbft_step_ << " PBFT block: " << pbft_block;
      // execute pbft schedule
      // TODO: VM executor will not take sortition_account_balance_table_tmp as
      //  reference. But will return a list of modified accounts as
      //  pairs<addr_t, val_t>.
      //  Will need update sortition_account_balance_table_tmp here
      uint64_t pbft_period = pbft_chain_->getPbftChainPeriod();
      if (!full_node->executePeriod(
              pbft_block, sortition_account_balance_table_tmp, pbft_period)) {
        LOG(log_err_) << "Failed to execute PBFT schedule";
      }
      db_->savePeriodScheduleBlock(pbft_period, pbft_block.getBlockHash());
      auto num_executed_blk = full_node->getNumBlockExecuted();
      auto num_executed_trx = full_node->getNumTransactionExecuted();
      if (num_executed_blk > 0 && num_executed_trx > 0) {
        db_->saveStatusField(StatusDbField::ExecutedBlkCount, num_executed_blk);
        db_->saveStatusField(StatusDbField::ExecutedTrxCount, num_executed_trx);
      }
      if (pbft_block.getSchedule().dag_blks_order.size() > 0) {
        auto write_batch = db_->createWriteBatch();
        for (auto const blk_hash : pbft_block.getSchedule().dag_blks_order) {
          db_->addDagBlockPeriodToBatch(blk_hash, pbft_period, write_batch);
        }
        db_->commitWriteBatch(write_batch);
      }
      // update sortition account balance table
      updateSortitionAccountsDB_();
      executed_pbft_block_ = true;
      return true;
    }
  }
  return false;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  uint64_t last_pbft_period = pbft_chain_->getPbftChainPeriod();
  int64_t since_period;
  if (last_pbft_period < SKIP_PERIODS) {
    since_period = 0;
  } else {
    since_period = last_pbft_period - SKIP_PERIODS;
  }
  size_t active_players = 0;
  while (active_players == 0 && since_period >= 0) {
    for (auto const &account : sortition_account_balance_table) {
      if (account.second.last_period_seen >= since_period) {
        active_players++;
      }
    }
    if (active_players == 0) {
      LOG(log_war_) << "Active players was found to be 0 since period "
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
    LOG(log_war_) << "Active players was found to be 0! This should only "
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
  LOG(log_inf_) << "Update 2t+1 " << TWO_T_PLUS_ONE << ", Threshold "
                << sortition_threshold_ << ", valid voting players "
                << valid_sortition_accounts_size_ << ", active players "
                << active_players << " since period " << since_period;
}

void PbftManager::updateSortitionAccountsTable_() {
  sortition_account_balance_table.clear();
  for (auto &account : sortition_account_balance_table_tmp) {
    sortition_account_balance_table[account.first] = account.second;
  }
  // update sortition accounts size
  valid_sortition_accounts_size_ = sortition_account_balance_table.size();
}

void PbftManager::updateSortitionAccountsDB_() {
  auto accounts = db_->createWriteBatch();
  for (auto &account : sortition_account_balance_table_tmp) {
    if (account.second.status == new_change) {
      db_->addSortitionAccountToBatch(account.first, account.second, accounts);
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
                                  account_size, accounts);
  db_->commitWriteBatch(accounts);
}

void PbftManager::countVotes_() {
  while (!monitor_stop_) {
    std::vector<Vote> votes = vote_mgr_->getAllVotes();

    size_t last_step_votes = 0;
    size_t current_step_votes = 0;
    for (auto const &v : votes) {
      if (pbft_step_ == 1) {
        if (v.getRound() == pbft_round_ - 1 && v.getStep() == last_step_) {
          last_step_votes++;
        } else if (v.getRound() == pbft_round_ && v.getStep() == pbft_step_) {
          current_step_votes++;
        }
      } else {
        if (v.getRound() == pbft_round_) {
          if (v.getStep() == pbft_step_ - 1) {
            last_step_votes++;
          } else if (v.getStep() == pbft_step_) {
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

    LOG(log_inf_test_) << "Round " << pbft_round_ << " step " << last_step_
                       << " time " << elapsed_last_step_time_in_ms
                       << "(ms) has " << last_step_votes << " votes";
    LOG(log_inf_test_) << "Round " << pbft_round_ << " step " << pbft_step_
                       << " time " << elapsed_current_step_time_in_ms
                       << "(ms) has " << current_step_votes << " votes";
    thisThreadSleepForMilliSeconds(100);
  }
}

}  // namespace taraxa
