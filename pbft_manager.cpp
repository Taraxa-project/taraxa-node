/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-07-26
 */

#include "pbft_manager.hpp"

#include "dag.hpp"
#include "full_node.hpp"
#include "libdevcore/SHA3.h"
#include "network.hpp"
#include "sortition.h"
#include "util.hpp"

#include <chrono>
#include <string>

namespace taraxa {

PbftManager::PbftManager(std::string const &genesis) : genesis_(genesis) {}
PbftManager::PbftManager(std::vector<uint> const &params,
                         std::string const &genesis)
    // TODO: for debug, need remove later
    : LAMBDA_ms(params[0]),
      COMMITTEE_SIZE(params[1]),
      VALID_SORTITION_COINS(params[2]),
      genesis_(genesis) {}

void PbftManager::setFullNode(shared_ptr<taraxa::FullNode> node) {
  node_ = node;
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return;
  }
  vote_mgr_ = full_node->getVoteManager();
  pbft_chain_ = full_node->getPbftChain();
  capability_ = full_node->getNetwork()->getTaraxaCapability();

  // Initialize master boot node account balance
  addr_t master_boot_node_address = full_node->getMasterBootNodeAddress();
  std::pair<val_t, bool> master_boot_node_account_balance =
      full_node->getBalance(master_boot_node_address);
  if (!master_boot_node_account_balance.second) {
    LOG(log_err_) << "Failed initial master boot node account balance."
                  << " Master boot node balance is not exist.";
  } else if (master_boot_node_account_balance.first != TARAXA_COINS_DECIMAL) {
    LOG(log_err_)
        << "Failed initial master boot node account balance. Current balance "
        << master_boot_node_account_balance.first;
  }
  sortition_account_balance_table[master_boot_node_address] =
      master_boot_node_account_balance.first;
}

void PbftManager::start() {
  if (!stopped_) {
    return;
  }
  auto full_node = node_.lock();

  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return;
  }
  db_votes_ = full_node->getVotesDB();
  db_pbftchain_ = full_node->getPbftChainDB();
  stopped_ = false;
  daemon_ = std::make_shared<std::thread>([this]() { run(); });
  LOG(log_inf_) << "A PBFT executor initiated ..." << std::endl;
}

void PbftManager::stop() {
  if (stopped_) {
    return;
  }
  db_votes_ = nullptr;
  db_pbftchain_ = nullptr;
  stopped_ = true;
  daemon_->join();
  daemon_.reset();
  LOG(log_inf_) << "A PBFT executor terminated ..." << std::endl;
  assert(daemon_ == nullptr);
}

/* When a node starts up it has to sync to the current phase (type of block
 * being generated) and step (within the block generation round)
 * Five step loop for block generation over three phases of blocks
 * User's credential, sigma_i_p for a round p is sig_i(R, p)
 * Leader l_i_p = min ( H(sig_j(R,p) ) over set of j in S_i where S_i is set of
 * users from which have received valid round p credentials
 */
void PbftManager::run() {
  // Initilize TWO_T_PLUS_ONE and sortition_threshold
  size_t accounts = sortition_account_balance_table.size();
  TWO_T_PLUS_ONE = accounts * 2 / 3 + 1;
  sortition_threshold_ = accounts;
  LOG(log_deb_) << "Initialize 2t+1 " << TWO_T_PLUS_ONE << " Threshold "
                << sortition_threshold_;

  auto round_clock_initial_datetime = std::chrono::system_clock::now();
  // <round, cert_voted_block_hash>
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_round;
  // <round, block_hash_added_into_chain>
  std::unordered_map<size_t, blk_hash_t> push_block_values_for_round;
  auto next_step_time_ms = 0;

  while (!stopped_) {
    auto now = std::chrono::system_clock::now();
    auto duration = now - round_clock_initial_datetime;
    auto elapsed_time_in_round_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    LOG(log_tra_) << "PBFT current round is " << pbft_round_;
    LOG(log_tra_) << "PBFT current step is " << pbft_step_;

    // Get votes
    std::vector<Vote> votes = vote_mgr_->getVotes(pbft_round_ - 1);

    // TODO: TEST CODE. Below is for adjust PBFT Lambda, will remove later
    std::string test_log = "PBFT current round " + std::to_string(pbft_round_) +
                           " step " + std::to_string(pbft_step_);
    std::map<uint64_t, size_t> round_votes;
    for (auto const &v : votes) {
      if (round_votes.find(v.getRound()) == round_votes.end()) {
        round_votes[v.getRound()] = 1;
      } else {
        round_votes[v.getRound()]++;
      }
    }
    for (auto &rv : round_votes) {
      test_log += ". In round " + std::to_string(rv.first) + " has votes " +
                  std::to_string(rv.second);
    }
    LOG(log_inf_test_) << test_log;
    // END TEST CODE

    blk_hash_t nodes_own_starting_value_for_round = NULL_BLOCK_HASH;

    // Check if we are synced to the right step ...
    size_t consensus_pbft_round = roundDeterminedFromVotes_(votes, pbft_round_);
    if (consensus_pbft_round != pbft_round_) {
      LOG(log_deb_) << "From votes determined round " << consensus_pbft_round;
      // p2p connection syncing should cover this situation, sync here for safe
      if (consensus_pbft_round > pbft_round_ + 1) {
        LOG(log_deb_)
            << "pbft chain behind, need broadcast request for missing blocks";
        syncPbftChainFromPeers_();
      }
      pbft_round_ = consensus_pbft_round;
      std::vector<Vote> cert_votes_for_round = getVotesOfTypeFromVotesForRound_(
          cert_vote_type, votes, pbft_round_ - 1,
          std::make_pair(NULL_BLOCK_HASH, false));
      std::pair<blk_hash_t, bool> cert_voted_block_hash =
          blockWithEnoughVotes_(cert_votes_for_round);
      if (cert_voted_block_hash.second) {
        // put pbft block into chain if have 2t+1 cert votes
        if (pushPbftBlockIntoChain_(pbft_round_ - 1,
                                    cert_voted_block_hash.first)) {
          push_block_values_for_round[pbft_round_ - 1] =
              cert_voted_block_hash.first;
        }
      }

      LOG(log_deb_) << "Advancing clock to pbft round " << pbft_round_
                    << ", step 1, and resetting clock.";
      // NOTE: This also sets pbft_step back to 1
      pbft_step_ = 1;
      round_clock_initial_datetime = std::chrono::system_clock::now();
      continue;
    }

    next_pbft_block_type_ = pbft_chain_->getNextPbftBlockType();

    if (pbft_step_ == 1) {
      // Value Proposal
      if (shouldSpeak(propose_vote_type, pbft_round_, pbft_step_)) {
        if (pbft_round_ == 1) {
          LOG(log_deb_) << "Proposing value of NULL_BLOCK_HASH "
                        << NULL_BLOCK_HASH << " for round 1 by protocol";
          placeVote_(NULL_BLOCK_HASH, propose_vote_type, pbft_round_,
                     pbft_step_);
        } else if (push_block_values_for_round.count(pbft_round_ - 1) ||
                   (pbft_round_ >= 2 &&
                    nullBlockNextVotedForRound_(votes, pbft_round_ - 1))) {
          // PBFT CS block only be proposed once in one period
          if (next_pbft_block_type_ != schedule_block_type ||
              !proposed_block_hash_.second) {
            // Propose value...
            proposed_block_hash_ = proposeMyPbftBlock_();
          }
          if (proposed_block_hash_.second) {
            placeVote_(proposed_block_hash_.first, propose_vote_type,
                       pbft_round_, pbft_step_);
          }
        } else if (pbft_round_ >= 2) {
          std::pair<blk_hash_t, bool> next_voted_block_from_previous_round =
              nextVotedBlockForRound_(votes, pbft_round_ - 1);
          if (next_voted_block_from_previous_round.second &&
              next_voted_block_from_previous_round.first != NULL_BLOCK_HASH) {
            LOG(log_deb_) << "Proposing next voted block "
                          << next_voted_block_from_previous_round.first
                          << " from previous round";
            placeVote_(next_voted_block_from_previous_round.first,
                       propose_vote_type, pbft_round_, pbft_step_);
          }
        }
      }
      next_step_time_ms = 2 * LAMBDA_ms;
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;
      pbft_step_ += 1;

    } else if (pbft_step_ == 2) {
      // The Filtering Step
      if (shouldSpeak(soft_vote_type, pbft_round_, pbft_step_)) {
        if (pbft_round_ == 1 ||
            (pbft_round_ >= 2 &&
             push_block_values_for_round.count(pbft_round_ - 1)) ||
            (pbft_round_ >= 2 &&
             nullBlockNextVotedForRound_(votes, pbft_round_ - 1))) {
          // Identity leader
          std::pair<blk_hash_t, bool> leader_block = identifyLeaderBlock_();
          if (leader_block.second) {
            LOG(log_deb_) << "Identify leader block " << leader_block.first
                          << " for round " << pbft_round_
                          << " and soft vote the value";
            placeVote_(leader_block.first, soft_vote_type, pbft_round_,
                       pbft_step_);
          }
        } else if (pbft_round_ >= 2) {
          std::pair<blk_hash_t, bool> next_voted_block_from_previous_round =
              nextVotedBlockForRound_(votes, pbft_round_ - 1);
          if (next_voted_block_from_previous_round.second &&
              next_voted_block_from_previous_round.first != NULL_BLOCK_HASH) {
            LOG(log_deb_) << "Soft voting "
                          << next_voted_block_from_previous_round.first
                          << " from previous round";
            placeVote_(next_voted_block_from_previous_round.first,
                       soft_vote_type, pbft_round_, pbft_step_);
          }
        }
      }

      next_step_time_ms = 2 * LAMBDA_ms;
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;
      pbft_step_ = pbft_step_ + 1;

    } else if (pbft_step_ == 3) {
      // The Certifying Step
      if (elapsed_time_in_round_ms < 2 * LAMBDA_ms) {
        // Should not happen, add log here for safety checking
        LOG(log_err_) << "PBFT Reached step 3 too quickly?";
      }

      bool should_go_to_step_four = false;  // TODO: may not need

      if (elapsed_time_in_round_ms > 4 * LAMBDA_ms - POLLING_INTERVAL_ms) {
        LOG(log_deb_) << "Reached step 3 late, will go to step 4";
        should_go_to_step_four = true;
      } else {
        LOG(log_tra_) << "In step 3";
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_round =
            softVotedBlockForRound_(votes, pbft_round_);
        if (soft_voted_block_for_this_round.second &&
            soft_voted_block_for_this_round.first != NULL_BLOCK_HASH &&
            (next_pbft_block_type_ != schedule_block_type ||
             comparePbftCSblockWithDAGblocks_(
                 soft_voted_block_for_this_round.first))) {
          cert_voted_values_for_round[pbft_round_] =
              soft_voted_block_for_this_round.first;
          should_go_to_step_four = true;
          if (checkPbftBlockValid_(soft_voted_block_for_this_round.first)) {
            if (shouldSpeak(cert_vote_type, pbft_round_, pbft_step_)) {
              LOG(log_deb_)
                  << "Cert voting " << soft_voted_block_for_this_round.first
                  << " for round " << pbft_round_;
              placeVote_(soft_voted_block_for_this_round.first, cert_vote_type,
                         pbft_round_, pbft_step_);
            }
          } else {
            // Get partition, need send request to get missing pbft blocks from
            // peers
            syncPbftChainFromPeers_();
          }
        }
      }

      if (should_go_to_step_four) {
        LOG(log_deb_) << "will go to step 4";
        next_step_time_ms = 4 * LAMBDA_ms;
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
                   nullBlockNextVotedForRound_(votes, pbft_round_ - 1)) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
        } else {
          LOG(log_deb_) << "Next voting nodes own starting value "
                        << nodes_own_starting_value_for_round << " for round "
                        << pbft_round_;
          placeVote_(nodes_own_starting_value_for_round, next_vote_type,
                     pbft_round_, pbft_step_);
        }
      }

      pbft_step_ += 1;
      next_step_time_ms = 4 * LAMBDA_ms;
      LOG(log_tra_) << "next step time(ms): " << next_step_time_ms;

    } else if (pbft_step_ == 5) {
      if (shouldSpeak(next_vote_type, pbft_round_, pbft_step_)) {
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_round =
            softVotedBlockForRound_(votes, pbft_round_);
        if (soft_voted_block_for_this_round.second &&
            soft_voted_block_for_this_round.first != NULL_BLOCK_HASH &&
            (next_pbft_block_type_ != schedule_block_type ||
             comparePbftCSblockWithDAGblocks_(
                 soft_voted_block_for_this_round.first))) {
          LOG(log_deb_) << "Next voting "
                        << soft_voted_block_for_this_round.first
                        << " for round " << pbft_round_;
          placeVote_(soft_voted_block_for_this_round.first, next_vote_type,
                     pbft_round_, pbft_step_);
        } else if (pbft_round_ >= 2 &&
                   nullBlockNextVotedForRound_(votes, pbft_round_ - 1) &&
                   (cert_voted_values_for_round.find(pbft_round_) ==
                    cert_voted_values_for_round.end())) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
        }
      }

      if (elapsed_time_in_round_ms > 6 * LAMBDA_ms - POLLING_INTERVAL_ms) {
        next_step_time_ms = 6 * LAMBDA_ms;
        pbft_step_ += 1;
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
                   nullBlockNextVotedForRound_(votes, pbft_round_ - 1)) {
          LOG(log_deb_) << "Next voting NULL BLOCK for round " << pbft_round_;
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
        } else {
          LOG(log_deb_) << "Next voting nodes own starting value for round "
                        << pbft_round_;
          placeVote_(nodes_own_starting_value_for_round, next_vote_type,
                     pbft_round_, pbft_step_);
        }
      }

      pbft_step_ += 1;

    } else {
      // Odd number steps 7, 9, 11... < MAX_STEPS are a repeat of step 5...
      if (shouldSpeak(next_vote_type, pbft_round_, pbft_step_)) {
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_round =
            softVotedBlockForRound_(votes, pbft_round_);
        if (soft_voted_block_for_this_round.second &&
            soft_voted_block_for_this_round.first != NULL_BLOCK_HASH &&
            (next_pbft_block_type_ != schedule_block_type ||
             comparePbftCSblockWithDAGblocks_(
                 soft_voted_block_for_this_round.first))) {
          LOG(log_deb_) << "Next voting "
                        << soft_voted_block_for_this_round.first
                        << " for round " << pbft_round_;
          placeVote_(soft_voted_block_for_this_round.first, next_vote_type,
                     pbft_round_, pbft_step_);
        } else if (pbft_round_ >= 2 &&
                   nullBlockNextVotedForRound_(votes, pbft_round_ - 1) &&
                   (cert_voted_values_for_round.find(pbft_round_) ==
                    cert_voted_values_for_round.end())) {
          LOG(log_deb_) << "Next voting NULL BLOCK for this round";
          placeVote_(NULL_BLOCK_HASH, next_vote_type, pbft_round_, pbft_step_);
        }
      }

      if (pbft_step_ >= MAX_STEPS) {
        pbft_round_ += 1;
        LOG(log_deb_) << "Having next voted, advancing clock to pbft round "
                      << pbft_round_ << ", step 1, and resetting clock.";
        // NOTE: This also sets pbft_step back to 1
        pbft_step_ = 1;
        round_clock_initial_datetime = std::chrono::system_clock::now();
        continue;
      } else if (elapsed_time_in_round_ms >
                 (pbft_step_ + 1) * LAMBDA_ms - POLLING_INTERVAL_ms) {
        next_step_time_ms = (pbft_step_ + 1) * LAMBDA_ms;
        pbft_step_ += 1;
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
    LOG(log_tra_) << "Time to sleep(ms): " << time_to_sleep_for_ms;
    if (time_to_sleep_for_ms > 0) {
      thisThreadSleepForMilliSeconds(time_to_sleep_for_ms);
    }
  }
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
    LOG(log_tra_) << "Don't have enough coins to vote";
    return false;
  }
  std::pair<val_t, bool> account_balance =
      full_node->getBalance(account_address);
  if (!account_balance.second) {
    LOG(log_tra_) << "Full node account unavailable" << std::endl;
    return false;
  }

  blk_hash_t last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  std::string message = last_pbft_block_hash.toString() + std::to_string(type) +
                        std::to_string(round) + std::to_string(step);

  dev::Signature sortition_signature = full_node->signMessage(message);
  string sortition_signature_hash = taraxa::hashSignature(sortition_signature);

  if (!taraxa::sortition(sortition_signature_hash, account_balance.first,
                         sortition_threshold_)) {
    LOG(log_tra_) << "Don't get sortition";
    return false;
  }
  return true;
}

/* Find the latest round, p-1, for which there is a quorum of next-votes
 * and set determine that round p should be the current round...
 */
size_t PbftManager::roundDeterminedFromVotes_(std::vector<Vote> &votes,
                                              uint64_t local_round) {
  // TODO: local_round may be able to change to pbft_round_, and remove
  // local_round tally next votes by round <vote_round, count>, round store in
  // reverse order
  std::map<size_t, size_t, std::greater<size_t>> next_votes_tally_by_round;

  for (Vote &v : votes) {
    if (v.getType() != next_vote_type) {
      continue;
    }

    size_t vote_round = v.getRound();
    if (vote_round >= local_round) {
      if (next_votes_tally_by_round.find(vote_round) !=
          next_votes_tally_by_round.end()) {
        next_votes_tally_by_round[vote_round] += 1;
      } else {
        next_votes_tally_by_round[vote_round] = 1;
      }
    }
  }

  for (auto &vp : next_votes_tally_by_round) {
    if (vp.second >= TWO_T_PLUS_ONE) {
      std::vector<Vote> next_votes_for_round = getVotesOfTypeFromVotesForRound_(
          next_vote_type, votes, vp.first,
          std::make_pair(NULL_BLOCK_HASH, false));
      if (blockWithEnoughVotes_(next_votes_for_round).second) {
        return vp.first + 1;
      }
    }
  }

  return local_round;
}

// Assumption is that all votes are in the same round and of same type...
std::pair<blk_hash_t, bool> PbftManager::blockWithEnoughVotes_(
    std::vector<Vote> &votes) {
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
        return std::make_pair(NULL_BLOCK_HASH, false);
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
      }
    }
  }

  return std::make_pair(NULL_BLOCK_HASH, false);
}

bool PbftManager::nullBlockNextVotedForRound_(std::vector<Vote> &votes,
                                              uint64_t round) {
  blk_hash_t blockhash = NULL_BLOCK_HASH;
  std::pair<blk_hash_t, bool> blockhash_pair = std::make_pair(blockhash, true);
  std::vector<Vote> votes_for_null_block_in_round =
      getVotesOfTypeFromVotesForRound_(next_vote_type, votes, round,
                                       blockhash_pair);
  if (votes_for_null_block_in_round.size() >= TWO_T_PLUS_ONE) {
    return true;
  }
  return false;
}

std::vector<Vote> PbftManager::getVotesOfTypeFromVotesForRound_(
    PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
    std::pair<blk_hash_t, bool> blockhash) {
  std::vector<Vote> votes_of_requested_type;

  for (Vote &v : votes) {
    if (v.getType() == vote_type && v.getRound() == round &&
        (blockhash.second == false || blockhash.first == v.getBlockHash())) {
      votes_of_requested_type.emplace_back(v);
    }
  }

  return votes_of_requested_type;
}

std::pair<blk_hash_t, bool> PbftManager::nextVotedBlockForRound_(
    std::vector<Vote> &votes, uint64_t round) {
  std::vector<Vote> next_votes_for_round = getVotesOfTypeFromVotesForRound_(
      next_vote_type, votes, round, std::make_pair(NULL_BLOCK_HASH, false));

  return blockWithEnoughVotes_(next_votes_for_round);
}

void PbftManager::placeVote_(taraxa::blk_hash_t const &blockhash,
                             PbftVoteTypes vote_type, uint64_t round,
                             size_t step) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return;
  }

  Vote vote = full_node->generateVote(blockhash, vote_type, round, step);
  vote_mgr_->addVote(vote);
  LOG(log_deb_) << "vote block hash: " << blockhash
                << " vote type: " << vote_type << " round: " << round
                << " step: " << step << " vote hash " << vote.getHash();
  // pbft vote broadcast
  full_node->broadcastVote(vote);
}

std::pair<blk_hash_t, bool> PbftManager::softVotedBlockForRound_(
    std::vector<taraxa::Vote> &votes, uint64_t round) {
  std::vector<Vote> soft_votes_for_round = getVotesOfTypeFromVotesForRound_(
      soft_vote_type, votes, round, std::make_pair(NULL_BLOCK_HASH, false));

  return blockWithEnoughVotes_(soft_votes_for_round);
}

std::pair<blk_hash_t, bool> PbftManager::proposeMyPbftBlock_() {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  PbftBlock pbft_block;
  if (next_pbft_block_type_ == pivot_block_type) {
    LOG(log_deb_) << "Into propose anchor block";
    blk_hash_t prev_pivot_hash = pbft_chain_->getLastPbftPivotHash();
    blk_hash_t prev_block_hash = pbft_chain_->getLastPbftBlockHash();
    // choose the last DAG block as PBFT pivot block in this period
    std::vector<std::string> ghost;
    full_node->getGhostPath(genesis_, ghost);
    blk_hash_t dag_block_hash(ghost.back());
    // compare with last dag block hash. If they are same, which means no new
    // dag blocks generated since last round In that case PBFT proposer should
    // propose NULL BLOCK HASH as their value and not produce a new block. In
    // practice this should never happen
    std::pair<PbftBlock, bool> last_round_pbft_anchor_block =
        pbft_chain_->getPbftBlockInChain(prev_pivot_hash);
    if (!last_round_pbft_anchor_block.second) {
      // Should not happen
      LOG(log_err_)
          << "Can not find the last round pbft pivot block with block hash: "
          << prev_pivot_hash;
      assert(false);
    }
    blk_hash_t last_round_dag_anchor_block_hash =
        last_round_pbft_anchor_block.first.getPivotBlock().getDagBlockHash();
    if (dag_block_hash == last_round_dag_anchor_block_hash) {
      LOG(log_deb_)
          << "Last round DAG anchor block hash " << dag_block_hash
          << " No new DAG blocks generated, PBFT propose NULL_BLOCK_HASH";
      return std::make_pair(NULL_BLOCK_HASH, true);
    }

    uint64_t propose_pbft_chain_period = pbft_chain_->getPbftChainPeriod() + 1;
    addr_t beneficiary = full_node->getAddress();
    // generate pivot block
    PivotBlock pivot_block(prev_pivot_hash, prev_block_hash, dag_block_hash,
                           propose_pbft_chain_period, beneficiary);
    // set pbft block as pivot
    pbft_block.setPivotBlock(pivot_block);

  } else if (next_pbft_block_type_ == schedule_block_type) {
    LOG(log_deb_) << "Into propose schedule block";
    // get dag block hash from the last pbft block(pivot) in pbft chain
    blk_hash_t last_block_hash = pbft_chain_->getLastPbftBlockHash();
    std::pair<PbftBlock, bool> last_pbft_block =
        pbft_chain_->getPbftBlockInChain(last_block_hash);
    if (!last_pbft_block.second) {
      // Should not happen
      LOG(log_err_) << "Can not find last pbft block with block hash: "
                    << last_block_hash;
      assert(false);
    }
    blk_hash_t dag_block_hash =
        last_pbft_block.first.getPivotBlock().getDagBlockHash();

    // get dag blocks order
    uint64_t pbft_chain_period;
    std::shared_ptr<vec_blk_t> dag_blocks_order;
    std::tie(pbft_chain_period, dag_blocks_order) =
        full_node->getDagBlockOrder(dag_block_hash);

    // get transactions overlap table,
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
        trx_overlap_table =
            full_node->computeTransactionOverlapTable(dag_blocks_order);
    if (!trx_overlap_table) {
      LOG(log_err_) << "Transaction overlap table nullptr, cannot create mock "
                    << "transactions schedule";
      return std::make_pair(NULL_BLOCK_HASH, false);
    }
    if (trx_overlap_table->empty()) {
      LOG(log_deb_) << "Transaction overlap table is empty, no DAG block needs "
                    << " generate mock trx schedule";
      return std::make_pair(NULL_BLOCK_HASH, false);
    }

    // TODO: generate fake transaction schedule for now, will pass
    //  trx_overlap_table to VM
    std::vector<std::vector<uint>> dag_blocks_trx_modes =
        full_node->createMockTrxSchedule(trx_overlap_table);
    TrxSchedule schedule(*dag_blocks_order, dag_blocks_trx_modes);

    // generate pbft schedule block
    ScheduleBlock schedule_block(last_block_hash, schedule);
    // set pbft block as schedule
    pbft_block.setScheduleBlock(schedule_block);
  }  // TODO: More pbft block types

  // setup timestamp for pbft block
  uint64_t timestamp = std::time(nullptr);
  pbft_block.setTimestamp(timestamp);

  // sign the pbft block
  std::string pbft_block_str = pbft_block.getJsonStr();
  sig_t signature = full_node->signMessage(pbft_block_str);
  pbft_block.setSignature(signature);
  pbft_block.setBlockHash();

  // push pbft block into pbft queue
  pbft_chain_->pushPbftBlockIntoQueue(pbft_block);
  // broadcast pbft block
  std::shared_ptr<Network> network = full_node->getNetwork();
  network->onNewPbftBlock(pbft_block);

  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  LOG(log_deb_) << "Propose succussful! block hash: " << pbft_block_hash
                << " in round: " << pbft_round_ << " in step: " << pbft_step_;

  return std::make_pair(pbft_block_hash, true);
}

std::pair<blk_hash_t, bool> PbftManager::identifyLeaderBlock_() {
  std::vector<Vote> votes = vote_mgr_->getVotes(pbft_round_);
  LOG(log_deb_) << "leader block type should be: " << next_pbft_block_type_;
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

bool PbftManager::pushPbftBlockIntoChain_(
    uint64_t round, taraxa::blk_hash_t const &cert_voted_block_hash) {
  std::vector<Vote> votes = vote_mgr_->getVotes(round);
  size_t count = 0;
  for (auto const &v : votes) {
    if (v.getBlockHash() == cert_voted_block_hash &&
        v.getType() == cert_vote_type && v.getRound() == round) {
      LOG(log_deb_) << "find cert vote " << v.getHash() << " for block hash "
                    << v.getBlockHash() << " in round " << v.getRound()
                    << " step " << v.getStep();
      count++;
    }
  }

  if (count < TWO_T_PLUS_ONE) {
    LOG(log_deb_) << "Not enough cert votes. Need " << TWO_T_PLUS_ONE
                  << " cert votes."
                  << " But only have " << count;
    return false;
  }

  if (!checkPbftBlockValid_(cert_voted_block_hash)) {
    // Get partition, need send request to get missing pbft blocks from peers
    syncPbftChainFromPeers_();
    return false;
  }
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getPbftBlockInQueue(cert_voted_block_hash);
  if (!pbft_block.second) {
    LOG(log_err_) << "Can not find the cert vote block hash "
                  << cert_voted_block_hash << " in pbft queue";
    return false;
  }

  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return false;
  }

  if (next_pbft_block_type_ == pivot_block_type) {
    if (pbft_chain_->pushPbftPivotBlock(pbft_block.first)) {
      // reset proposed PBFT block hash to False for next CS block proposal
      proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);
      updatePbftChainDB_(pbft_block.first);
      LOG(log_deb_) << "Successful push pbft anchor block "
                    << pbft_block.first.getBlockHash() << " into chain!";
      // get dag blocks order
      blk_hash_t dag_block_hash =
          pbft_block.first.getPivotBlock().getDagBlockHash();
      uint64_t current_period;
      std::shared_ptr<vec_blk_t> dag_blocks_order;
      std::tie(current_period, dag_blocks_order) =
          full_node->getDagBlockOrder(dag_block_hash);

      // update DAG blocks order and DAG blocks table
      for (auto const &dag_blk_hash : *dag_blocks_order) {
        pbft_chain_->pushDagBlockHash(dag_blk_hash);
      }

      return true;
    }
  } else if (next_pbft_block_type_ == schedule_block_type) {
    if (pbft_chain_->pushPbftScheduleBlock(pbft_block.first)) {
      updatePbftChainDB_(pbft_block.first);
      LOG(log_deb_) << "Successful push pbft schedule block "
                    << pbft_block.first.getBlockHash() << " into chain!";

      // set DAG blocks period
      blk_hash_t last_pivot_block_hash = pbft_chain_->getLastPbftPivotHash();
      std::pair<PbftBlock, bool> last_pivot_block =
          pbft_chain_->getPbftBlockInChain(last_pivot_block_hash);
      if (!last_pivot_block.second) {
        LOG(log_err_) << "Cannot find the last pivot block hash "
                      << last_pivot_block_hash
                      << " in pbft chain. Should never happen here!";
        assert(false);
      }
      blk_hash_t dag_block_hash =
          last_pivot_block.first.getPivotBlock().getDagBlockHash();
      uint64_t current_pbft_chain_period =
          last_pivot_block.first.getPivotBlock().getPeriod();
      uint dag_ordered_blocks_size = full_node->setDagBlockOrder(
          dag_block_hash, current_pbft_chain_period);
      // checking: DAG ordered blocks size in this period should equal to the
      // DAG blocks inside PBFT CS block
      uint dag_blocks_inside_pbft_cs =
          pbft_block.first.getScheduleBlock().getSchedule().blk_order.size();
      if (dag_ordered_blocks_size != dag_blocks_inside_pbft_cs) {
        LOG(log_err_) << "Setting DAG block order finalize "
                      << dag_ordered_blocks_size << " blocks."
                      << " But the PBFT CS block has "
                      << dag_blocks_inside_pbft_cs << " DAG block hash.";
        // TODO: need to handle the error condition(should never happen)
      }

      // execute schedule block
      // TODO: VM executor will not take sortition_account_balance_table as
      // reference.
      //  But will return a list of modified accounts as pairs<addr_t, val_t>.
      //  Will need update sortition_account_balance_table here
      if (!full_node->executeScheduleBlock(pbft_block.first.getScheduleBlock(),
                                           sortition_account_balance_table)) {
        LOG(log_err_) << "Failed to execute schedule block";
        // TODO: If valid transaction failed execute, how to do?
      }

      // reset sortition_threshold and TWO_T_PLUS_ONE
      size_t accounts = sortition_account_balance_table.size();
      if (COMMITTEE_SIZE <= accounts) {
        TWO_T_PLUS_ONE = COMMITTEE_SIZE * 2 / 3 + 1;
        sortition_threshold_ = COMMITTEE_SIZE;
      } else {
        TWO_T_PLUS_ONE = accounts * 2 / 3 + 1;
        sortition_threshold_ = accounts;
      }
      LOG(log_deb_) << "Update 2t+1 " << TWO_T_PLUS_ONE << " Threshold "
                    << sortition_threshold_;

      return true;
    }
  }  // TODO: more pbft block type

  return false;
}

bool PbftManager::updatePbftChainDB_(PbftBlock const &pbft_block) {
  if (!db_pbftchain_->put(pbft_block.getBlockHash().toString(),
                          pbft_block.getJsonStr())) {
    LOG(log_err_) << "Failed put pbft block: " << pbft_block.getBlockHash()
                  << " into DB";
    return false;
  }
  if (!db_pbftchain_->update(pbft_chain_->getGenesisHash().toString(),
                             pbft_chain_->getJsonStr())) {
    LOG(log_err_) << "Failed update pbft genesis in DB";
    return false;
  }
  db_pbftchain_->commit();

  return true;
}

bool PbftManager::checkPbftBlockValid_(blk_hash_t const &block_hash) const {
  std::pair<PbftBlock, bool> cert_voted_block =
      pbft_chain_->getPbftBlockInQueue(block_hash);
  if (!cert_voted_block.second) {
    LOG(log_inf_) << "Cannot find the pbft block hash in queue, block hash "
                  << block_hash;
    return false;
  }
  PbftBlockTypes cert_voted_block_type = cert_voted_block.first.getBlockType();
  if (next_pbft_block_type_ != cert_voted_block_type) {
    LOG(log_inf_) << "Pbft chain next pbft block type should be "
                  << next_pbft_block_type_ << " Invalid pbft block type "
                  << cert_voted_block.first.getBlockType();
    return false;
  }
  if (cert_voted_block_type == pivot_block_type) {
    blk_hash_t prev_pivot_block_hash =
        cert_voted_block.first.getPivotBlock().getPrevPivotBlockHash();
    if (pbft_chain_->getLastPbftPivotHash() != prev_pivot_block_hash) {
      LOG(log_inf_) << "Pbft chain last pivot block hash "
                    << pbft_chain_->getLastPbftPivotHash()
                    << " Invalid pbft prev pivot block hash "
                    << prev_pivot_block_hash;
      return false;
    }
    blk_hash_t prev_block_hash =
        cert_voted_block.first.getPivotBlock().getPrevBlockHash();
    if (pbft_chain_->getLastPbftBlockHash() != prev_block_hash) {
      LOG(log_inf_) << "Pbft chain last block hash "
                    << pbft_chain_->getLastPbftBlockHash()
                    << " Invalid pbft prev block hash " << prev_block_hash;
      return false;
    }
  } else if (cert_voted_block_type == schedule_block_type) {
    blk_hash_t prev_block_hash =
        cert_voted_block.first.getScheduleBlock().getPrevBlockHash();
    if (pbft_chain_->getLastPbftBlockHash() != prev_block_hash) {
      LOG(log_inf_) << "Pbft chain last block hash "
                    << pbft_chain_->getLastPbftBlockHash()
                    << " Invalid pbft prev block hash " << prev_block_hash;
      return false;
    }
  }  // TODO: More pbft block types

  return true;
}

void PbftManager::syncPbftChainFromPeers_() {
  vector<NodeID> peers = capability_->getAllPeers();
  if (peers.empty()) {
    LOG(log_err_) << "There is no peers with connection.";
  } else {
    for (auto &peer : peers) {
      LOG(log_deb_) << "In round " << pbft_round_
                    << " sync pbft chain with node " << peer
                    << " Send request to ask missing pbft blocks in chain";
      capability_->syncPeerPbft(peer);
    }
  }
}

bool PbftManager::comparePbftCSblockWithDAGblocks_(
    blk_hash_t const &cs_block_hash) {
  std::pair<PbftBlock, bool> cs_block =
      pbft_chain_->getPbftBlockInQueue(cs_block_hash);
  if (!cs_block.second) {
    LOG(log_inf_) << "Have not got the PBFT CS block yet. block hash: "
                  << cs_block_hash;
    return false;
  }

  // get dag block hash from the last pbft pivot block in pbft chain
  blk_hash_t last_block_hash = pbft_chain_->getLastPbftPivotHash();
  std::pair<PbftBlock, bool> last_pbft_block =
      pbft_chain_->getPbftBlockInChain(last_block_hash);
  if (!last_pbft_block.second) {
    // Should not happen
    LOG(log_err_) << "Can not find last pbft block with block hash: "
                  << last_block_hash;
    assert(false);
  }
  blk_hash_t dag_block_hash =
      last_pbft_block.first.getPivotBlock().getDagBlockHash();
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return false;
  }
  // get dag blocks order
  uint64_t pbft_chain_period;
  std::shared_ptr<vec_blk_t> dag_blocks_order;
  std::tie(pbft_chain_period, dag_blocks_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // compare blocks hash in CS with DAG blocks
  vec_blk_t blocks_in_cs =
      cs_block.first.getScheduleBlock().getSchedule().blk_order;
  if (blocks_in_cs.size() == dag_blocks_order->size()) {
    for (auto i = 0; i < blocks_in_cs.size(); i++) {
      if (blocks_in_cs[i] != (*dag_blocks_order)[i]) {
        LOG(log_err_) << "Block hash: " << blocks_in_cs[i]
                      << " in PBFT CS is different with DAG block hash "
                      << (*dag_blocks_order)[i];
        return false;
      }
    }
  } else {
    LOG(log_inf_) << "DAG blocks have not sync yet. in period: "
                  << pbft_chain_period
                  << " PBFT CS blocks size: " << blocks_in_cs.size()
                  << " DAG blocks size: " << dag_blocks_order->size();
    return false;
  }
  // compare number of transactions in CS with DAG blocks
  // PBFT CS block number of transactions
  std::vector<std::vector<uint>> trx_modes =
      cs_block.first.getScheduleBlock().getSchedule().vec_trx_modes;
  for (int i = 0; i < dag_blocks_order->size(); i++) {
    std::shared_ptr<DagBlock> dag_block =
        full_node->getDagBlock((*dag_blocks_order)[i]);
    // DAG block transations
    vec_trx_t dag_block_trxs = dag_block->getTrxs();
    if (trx_modes[i].size() != dag_block_trxs.size()) {
      LOG(log_err_) << "In DAG block hash: " << (*dag_blocks_order)[i]
                    << " has " << dag_block_trxs.size()
                    << " transactions. But the DAG block in PBFT CS block only"
                    << " has " << trx_modes[i].size() << " transactions.";
      return false;
    }
  }

  return true;
}

}  // namespace taraxa
