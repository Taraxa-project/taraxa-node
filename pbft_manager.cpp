/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-04-23
 */

#include "pbft_manager.hpp"

#include "full_node.hpp"
#include "libdevcore/SHA3.h"
#include "sortition.h"
#include "util.hpp"

#include <chrono>
#include <string>

namespace taraxa {

PbftManager::PbftManager() : vote_queue_(std::make_shared<VoteQueue>()) {}

void PbftManager::start() {
  if (!stopped_) {
    return;
  }
  stopped_ = false;
  executor_ = std::make_shared<std::thread>([this]() { run(); });
  LOG(log_inf_) << "A PBFT executor initiated ..." << std::endl;
}

void PbftManager::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;
  executor_->join();
  executor_.reset();
  LOG(log_inf_) << "A PBFT executor terminated ..." << std::endl;
  assert(executor_ == nullptr);
}

/* When a node starts up it has to sync to the current phase (type of block
 * being generated) and step (within the block generation period)
 * Five step loop for block generation over three phases of blocks
 * User's credential, sigma_i_p for a period p is sig_i(R, p)
 * Leader l_i_p = min ( H(sig_j(R,p) ) over set of j in S_i where S_i is set of
 * users from which have received valid period p credentials
 */
void PbftManager::run() {
  auto period_clock_initial_datetime = std::chrono::system_clock::now();
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_period;

  while (!stopped_) {
    auto now = std::chrono::system_clock::now();
    auto duration = now - period_clock_initial_datetime;
    auto elapsed_time_in_round_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    auto next_step_time_ms = 0;

    LOG(log_tra_) << "PBFT period is " << pbft_period_;
    LOG(log_tra_) << "PBFT step is " << pbft_step_;

    // Get votes
    std::vector<Vote> votes = vote_queue_->getVotes(pbft_period_ - 1);

    blk_hash_t nodes_own_starting_value_for_period = NULL_BLOCK_HASH;

    // Check if we are synced to the right step ...
    size_t consensus_pbfy_period =
        periodDeterminedFromVotes_(votes, pbft_period_);

    if (consensus_pbfy_period != pbft_period_) {
      LOG(log_tra_) << "Period determined from votes: " << consensus_pbfy_period;
      pbft_period_ = consensus_pbfy_period;
      LOG(log_tra_) << "Advancing clock to pbft period " << pbft_period_
                   << ", step 1, and resetting clock.";
      // NOTE: This also sets pbft_step back to 1
      pbft_step_ = 1;
      period_clock_initial_datetime = std::chrono::system_clock::now();
      continue;
    }

    if (pbft_step_ == 1) {
      // Value Proposal
      if (pbft_period_ == 1 ||
          (pbft_period_ >= 2 &&
           nullBlockNextVotedForPeriod_(votes, pbft_period_ - 1))) {
        // Propose value...
        LOG(log_tra_) << "Propose my value...";
      } else if (pbft_period_ >= 2) {
        std::pair<blk_hash_t, bool> next_voted_block_from_previous_period =
            nextVotedBlockForPeriod_(votes, pbft_period_ - 1);
        if (next_voted_block_from_previous_period.second) {
          LOG(log_tra_) << "Proposing next voted block "
                        << next_voted_block_from_previous_period.first
                        << " from previous period.";
        }
      }
      next_step_time_ms = 2 * LAMBDA_ms;
      pbft_step_ += 1;

    } else if (pbft_step_ == 2) {
      // The Filtering Step
      if (pbft_period_ == 1 ||
          (pbft_period_ >= 2 &&
           nullBlockNextVotedForPeriod_(votes, pbft_period_ - 1))) {
        // Identity leader
        LOG(log_tra_) << "Identify leader l_i_p for period p and soft vote the"
                        " value that they proposed...";
      } else if (pbft_period_ >= 2) {
        std::pair<blk_hash_t, bool> next_voted_block_from_previous_period =
            nextVotedBlockForPeriod_(votes, pbft_period_ - 1);
        if (next_voted_block_from_previous_period.second) {
          LOG(log_tra_) << "Soft voting "
                        << next_voted_block_from_previous_period.first
                        << " from previous period";
          placeVoteIfCanSpeak_(next_voted_block_from_previous_period.first,
                               soft_vote_type, pbft_period_, pbft_step_, false);
        }
      }

      next_step_time_ms = 2 * LAMBDA_ms;
      pbft_step_ = pbft_step_ + 1;

    } else if (pbft_step_ == 3) {
      // The Certifying Step
      if (elapsed_time_in_round_ms > 2 * LAMBDA_ms) {
        LOG(log_tra_) << "PBFT Reached step 3 too quickly?";
      }

      bool should_go_to_step_four = false;

      if (elapsed_time_in_round_ms > 4 * LAMBDA_ms - POLLING_INTERVAL_ms) {
        should_go_to_step_four = true;
      } else {
        std::pair<blk_hash_t, bool> soft_voted_block_for_this_period =
            softVotedBlockForPeriod_(votes, pbft_period_);
        if (soft_voted_block_for_this_period.second) {
          should_go_to_step_four = true;
          LOG(log_tra_) << "Cert voting "
                       << soft_voted_block_for_this_period.first
                       << " for this period";
          cert_voted_values_for_period[pbft_period_] =
              soft_voted_block_for_this_period.first;
          placeVoteIfCanSpeak_(soft_voted_block_for_this_period.first,
                               cert_vote_type, pbft_period_, pbft_step_, false);
        }
      }

      if (should_go_to_step_four) {
        next_step_time_ms = 4 * LAMBDA_ms;
        pbft_step_ += 1;
      } else {
        next_step_time_ms = next_step_time_ms + POLLING_INTERVAL_ms;
      }

    } else if (pbft_step_ == 4) {
      if (cert_voted_values_for_period.find(pbft_period_) !=
          cert_voted_values_for_period.end()) {
        LOG(log_tra_) << "Next voting value "
                      << cert_voted_values_for_period[pbft_period_]
                      << " for period " << pbft_period_;
        placeVoteIfCanSpeak_(cert_voted_values_for_period[pbft_period_],
                             next_vote_type, pbft_period_, pbft_step_, false);
      } else if (pbft_period_ >= 2 &&
                 nullBlockNextVotedForPeriod_(votes, pbft_period_ - 1)) {
        LOG(log_tra_) << "Next voting NULL BLOCK for period " << pbft_period_;
        placeVoteIfCanSpeak_(NULL_BLOCK_HASH, next_vote_type, pbft_period_,
                             pbft_step_, false);
      } else {
        LOG(log_tra_) << "Next voting nodes own starting value for period "
                      << pbft_period_;
        placeVoteIfCanSpeak_(nodes_own_starting_value_for_period,
                             next_vote_type, pbft_period_, pbft_step_, false);
      }

      pbft_step_ += 1;
      next_step_time_ms = 4 * LAMBDA_ms;

    } else if (pbft_step_ == 5) {
      std::pair<blk_hash_t, bool> soft_voted_block_for_this_period =
          softVotedBlockForPeriod_(votes, pbft_period_);
      bool have_next_voted = false;

      if (soft_voted_block_for_this_period.second) {
        LOG(log_tra_) << "Next voting "
                      << soft_voted_block_for_this_period.first
                      << " for this period";
        have_next_voted = true;
        placeVoteIfCanSpeak_(soft_voted_block_for_this_period.first,
                             next_vote_type, pbft_period_, pbft_step_, false);
      } else if (pbft_period_ >= 2 &&
                 nullBlockNextVotedForPeriod_(votes, pbft_period_ - 1) &&
                 (cert_voted_values_for_period.find(pbft_period_) ==
                  cert_voted_values_for_period.end())) {
        LOG(log_tra_) << "Next voting NULL BLOCK for this period";
        have_next_voted = true;
        placeVoteIfCanSpeak_(NULL_BLOCK_HASH, next_vote_type, pbft_period_,
                             pbft_step_, false);
      }

      if (have_next_voted) {
        pbft_period_ += 1;
        LOG(log_tra_) << "Having next voted, advancing clock to pbft period "
                      << pbft_period_ << ", step 1, and resetting clock.";
        // NOTE: This also sets pbft_step back to 1
        pbft_step_ = 1;
        period_clock_initial_datetime = std::chrono::system_clock::now();
        continue;
      } else if (elapsed_time_in_round_ms >
                 6 * LAMBDA_ms - POLLING_INTERVAL_ms) {
        next_step_time_ms = 6 * LAMBDA_ms;
        pbft_step_ += 1;
      } else {
        next_step_time_ms += POLLING_INTERVAL_ms;
      }

    } else if (pbft_step_ % 2 == 0) {
      // Even number steps 6, 8, 10... < MAX_STEPS are a repeat of step 4...
      if (cert_voted_values_for_period.find(pbft_period_) !=
          cert_voted_values_for_period.end()) {
        LOG(log_tra_) << "Next voting value "
                      << cert_voted_values_for_period[pbft_period_]
                      << " for period " << pbft_period_;
        placeVoteIfCanSpeak_(cert_voted_values_for_period[pbft_period_],
                             next_vote_type, pbft_period_, pbft_step_, false);
      } else if (pbft_period_ >= 2 &&
                 nullBlockNextVotedForPeriod_(votes, pbft_period_ - 1)) {
        LOG(log_tra_) << "Next voting NULL BLOCK for period " << pbft_period_;
        placeVoteIfCanSpeak_(NULL_BLOCK_HASH, next_vote_type, pbft_period_,
                             pbft_step_, false);
      } else {
        LOG(log_tra_) << "Next voting nodes own starting value for period "
                      << pbft_period_;
        placeVoteIfCanSpeak_(nodes_own_starting_value_for_period,
                             next_vote_type, pbft_period_, pbft_step_, false);
      }

      pbft_step_ += 1;

    } else {
      // Odd number steps 7, 9, 11... < MAX_STEPS are a repeat of step 5...
      std::pair<blk_hash_t, bool> soft_voted_block_for_this_period =
          softVotedBlockForPeriod_(votes, pbft_period_);
      bool have_next_voted = false;

      if (soft_voted_block_for_this_period.second) {
        LOG(log_tra_) << "Next voting " << soft_voted_block_for_this_period.first
                     << " for this period";
        have_next_voted = true;
        placeVoteIfCanSpeak_(soft_voted_block_for_this_period.first,
                             next_vote_type, pbft_period_, pbft_step_, false);
      } else if (pbft_period_ >= 2 &&
                 nullBlockNextVotedForPeriod_(votes, pbft_period_ - 1) &&
                 (cert_voted_values_for_period.find(pbft_period_) ==
                  cert_voted_values_for_period.end())) {
        LOG(log_tra_) << "Next voting NULL BLOCK for this period";
        have_next_voted = true;
        placeVoteIfCanSpeak_(NULL_BLOCK_HASH, next_vote_type, pbft_period_,
                             pbft_step_, false);
      }

      if (have_next_voted || pbft_step_ >= MAX_STEPS) {
        pbft_period_ += 1;
        LOG(log_tra_) << "Having next voted, advancing clock to pbft period "
                      << pbft_period_ << ", step 1, and resetting clock.";
        // NOTE: This also sets pbft_step back to 1
        pbft_step_ = 1;
        period_clock_initial_datetime = std::chrono::system_clock::now();
        continue;
      } else if (elapsed_time_in_round_ms >
                 (pbft_step_ + 1) * LAMBDA_ms - POLLING_INTERVAL_ms) {
        next_step_time_ms = (pbft_step_ + 1) * LAMBDA_ms;
        pbft_step_ += 1;
      } else {
        next_step_time_ms += POLLING_INTERVAL_ms;
      }
    }

    now = std::chrono::system_clock::now();
    duration = now - period_clock_initial_datetime;
    elapsed_time_in_round_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    auto time_to_sleep_for_ms = next_step_time_ms - elapsed_time_in_round_ms;
    if (time_to_sleep_for_ms > 0) {
      thisThreadSleepForMilliSeconds(time_to_sleep_for_ms);
    }
  }
}

bool PbftManager::shouldSpeak(blk_hash_t const &blockhash, char type,
                              int period, int step) {
  std::string message = blockhash.toString() + std::to_string(type) +
                        std::to_string(period) + std::to_string(step);
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return false;
  }
  dev::Signature signature = full_node->signMessage(message);
  string signature_hash = taraxa::hashSignature(signature);
  std::pair<bal_t, bool> account_balance =
      full_node->getBalance(full_node->getAddress());
  if (!account_balance.second) {
    LOG(log_war_) << "Full node account unavailable" << std::endl;
    return false;
  }
  if (taraxa::sortition(signature_hash, account_balance.first)) {
    LOG(log_inf_) << "Win sortition" << std::endl;
    return true;
  } else {
    return false;
  }
}

/* Find the latest period, p-1, for which there is a quorum of next-votes
 * and set determine that period p should be the current period...
 */
size_t PbftManager::periodDeterminedFromVotes_(std::vector<Vote> &votes,
                                               size_t local_period) {
  // tally next votes by period
  // store in reverse order
  std::map<size_t, size_t, std::greater<size_t> > next_votes_tally_by_period;

  for (Vote &v : votes) {
    if (v.getType() != next_vote_type) {
      continue;
    }

    size_t vote_period = v.getPeriod();
    if (vote_period >= local_period) {
      if (next_votes_tally_by_period.find(vote_period) !=
          next_votes_tally_by_period.end()) {
        next_votes_tally_by_period[vote_period] += 1;
      } else {
        next_votes_tally_by_period[vote_period] = 1;
      }
    }
  }

  for (auto &vp : next_votes_tally_by_period) {
    if (vp.second >= TWO_T_PLUS_ONE) {
      std::vector<Vote> next_vote_for_period =
          getVotesOfTypeFromPeriod_(next_vote_type, votes, vp.first,
                                    std::make_pair(blk_hash_t(0), false));
      if (blockWithEnoughVotes_(next_vote_for_period).second) {
        return vp.first + 1;
      }
    }
  }

  return local_period;
}

std::vector<Vote> PbftManager::getVotesOfTypeFromPeriod_(
    int vote_type, std::vector<Vote> &votes, size_t period,
    std::pair<blk_hash_t, bool> blockhash) {
  // We should read through the votes ...
  std::vector<Vote> votes_of_requested_type;

  for (Vote &v : votes) {
    if (v.getType() == vote_type && v.getPeriod() == period &&
        (blockhash.second == false || blockhash.first == v.getBlockHash())) {
      votes_of_requested_type.emplace_back(v);
    }
  }

  return votes_of_requested_type;
}

// Assumption is that all votes are in the same period and of same type...
std::pair<blk_hash_t, bool> PbftManager::blockWithEnoughVotes_(
    std::vector<Vote> &votes) {
  bool is_first_block = true;
  size_t vote_type;
  size_t vote_period;
  blk_hash_t blockhash;
  // store in reverse order
  std::map<blk_hash_t, size_t, std::greater<blk_hash_t> > tally_by_blockhash;

  for (Vote &v : votes) {
    if (is_first_block) {
      vote_type = v.getType();
      vote_period = v.getPeriod();
      is_first_block = false;
    } else {
      bool vote_type_and_period_is_consistent =
          (vote_type == v.getType() && vote_period == v.getPeriod());
      if (!vote_type_and_period_is_consistent) {
        LOG(log_err_) << "Vote types and periods were not internally"
                         " consistent!";
        assert(false);
      }
    }

    blockhash = v.getBlockHash();
    if (tally_by_blockhash.find(blockhash) != tally_by_blockhash.end()) {
      tally_by_blockhash[blockhash] += 1;
    } else {
      tally_by_blockhash[blockhash] = 1;
    }

    for (auto &blockhash_pair : tally_by_blockhash) {
      if (blockhash_pair.second >= TWO_T_PLUS_ONE) {
        return std::make_pair(blockhash_pair.first, true);
      }
    }
  }

  return std::make_pair(blk_hash_t(0), false);
}

bool PbftManager::nullBlockNextVotedForPeriod_(std::vector<Vote> &votes,
                                               size_t period) {
  blk_hash_t blockhash = NULL_BLOCK_HASH;
  std::pair<blk_hash_t, bool> blockhash_pair = std::make_pair(blockhash, true);
  std::vector<Vote> votes_for_null_block_in_period =
      getVotesOfTypeFromVotesForPeriod_(next_vote_type, votes, period,
                                        blockhash_pair);
  if (votes_for_null_block_in_period.size() >= TWO_T_PLUS_ONE) {
    return true;
  }
  return false;
}

std::vector<Vote> PbftManager::getVotesOfTypeFromVotesForPeriod_(
    int vote_type, std::vector<Vote> &votes, size_t period,
    std::pair<blk_hash_t, bool> blockhash) {
  std::vector<Vote> votes_of_requested_type;

  for (Vote &v : votes) {
    if (v.getType() == vote_type && v.getPeriod() == period &&
        (blockhash.second == false || blockhash.first == v.getBlockHash())) {
      votes_of_requested_type.emplace_back(v);
    }
  }

  return votes_of_requested_type;
}

std::pair<blk_hash_t, bool> PbftManager::nextVotedBlockForPeriod_(
    std::vector<Vote> &votes, size_t period) {
  std::vector<Vote> next_votes_for_period = getVotesOfTypeFromVotesForPeriod_(
      next_vote_type, votes, period, std::make_pair(blk_hash_t(0), false));

  return blockWithEnoughVotes_(next_votes_for_period);
}

void PbftManager::placeVoteIfCanSpeak_(taraxa::blk_hash_t blockhash,
                                       int vote_type, size_t period,
                                       size_t step,
                                       bool override_sortition_check) {
  bool should_i_speak_response = true;

  if (!override_sortition_check) {
    should_i_speak_response = shouldSpeak(blockhash, vote_type, period, step);
  }
  if (should_i_speak_response) {
    auto full_node = node_.lock();
    if (!full_node) {
      LOG(log_err_) << "Full node unavailable" << std::endl;
      return;
    }
    full_node->placeVote(blockhash, vote_type, period, step);
  }
}

std::pair<blk_hash_t, bool> PbftManager::softVotedBlockForPeriod_(
    std::vector<taraxa::Vote> &votes, size_t period) {
  std::vector<Vote> soft_votes_for_period = getVotesOfTypeFromVotesForPeriod_(
      soft_vote_type, votes, period, std::make_pair(blk_hash_t(0), false));

  return blockWithEnoughVotes_(soft_votes_for_period);
}

}  // namespace taraxa
