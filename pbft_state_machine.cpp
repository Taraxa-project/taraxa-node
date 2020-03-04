/*
 * @Copyright: Taraxa.io
 * @Author: Kelly Martines
 * @Date: 2020-03-01
 * @Last Mo-dified by: Kelly Martines
 * @Last Modified time: 2020-03-01
 */

#include "pbft_state_machine.hpp"

#include <libdevcore/SHA3.h>

#include <chrono>
#include <string>

#include "dag.hpp"
#include "full_node.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "sortition.hpp"
#include "util.hpp"
#include "util/eth.hpp"

namespace taraxa {

PbftStateMachine::PbftStateMachine(PbftManager* pbft_mgr)
    : pbft_mgr_(pbft_mgr) {}

void PbftStateMachine::start() {
  if (bool b = true; !stop_.compare_exchange_strong(b, !b)) {
    return;
  }
  daemon_ = std::make_unique<std::thread>([this]() { run(); });
  LOG(log_deb_) << "PBFT state machine initiated ...";
}

void PbftStateMachine::stop() {
  if (bool b = false; !stop_.compare_exchange_strong(b, !b)) {
    return;
  }
  daemon_->join();
  LOG(log_deb_) << "PBFT state machine terminated ...";
}

void PbftStateMachine::run() {
  LOG(log_inf_) << "PBFT state machine running ...";

  pbft_mgr_->resetRound();

  std::pair<blk_hash_t, bool> soft_voted_block_for_this_round_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  while (!stop_) {
    pbft_mgr_->syncChain();
    pbft_mgr_->setState(getNextState());
    pbft_mgr_->executeState();
    // Sleep until end of state
    auto sleep_time_ms = pbft_mgr_->getNextStateCheckTimeMs();
    if (sleep_time_ms) {
      LOG(log_tra_) << "Sleep time(ms): " << sleep_time_ms << " in round "
                    << pbft_mgr_->round_ << ", step " << pbft_mgr_->state_;
      thisThreadSleepForMilliSeconds(sleep_time_ms);
    }
  }
}

int PbftStateMachine::getNextState() {
  switch (pbft_mgr_->state_) {
    // Step 0
    case reset_state:
      switch (pbft_mgr_->round_) {
        case 0:
          return reset_state;
        case 1:
          return roll_call_state;
        default:
          return (pbft_mgr_->hasNextVotedBlockFromLastRound())
                     ? propose_block_from_prev_round_state
                     : propose_block_from_self_state;
      }
    // Step 1
    case roll_call_state:
    case propose_block_from_self_state:
      return soft_vote_block_from_leader;
    case propose_block_from_prev_round_state:
      return soft_vote_block_from_prev_round_state;
    // Step 2
    case soft_vote_block_from_leader:
    case soft_vote_block_from_prev_round_state:
      return cert_vote_block_polling_state;
    // Step 3
    case cert_vote_block_polling_state:
      if (pbft_mgr_->have_cert_voted_this_round_) {
        return chain_block_state;
      }
      if (pbft_mgr_->getStateElapsedTimeMs() > pbft_mgr_->state_max_time_ms_) {
        LOG(log_deb_) << "Certification state time expired in round: "
                      << pbft_mgr_->round_;
        return chain_block_state;
      }
      return cert_vote_block_polling_state;
    // Step 4
    case chain_block_state:
      if (pbft_mgr_->getStateElapsedTimeMs() > pbft_mgr_->state_max_time_ms_) {
        LOG(log_deb_) << "Pushing Block state time expired in round: "
                      << pbft_mgr_->round_;
        // We skip vote_next_block_state due to having missed it while
        // pushing a block....
        return vote_next_block_polling_state;
      }
      return vote_next_block_state;
    // Step 5...
    case vote_next_block_state:
      return vote_next_block_polling_state;
    case vote_next_block_polling_state:
      if (pbft_mgr_->getStateElapsedTimeMs() > pbft_mgr_->state_max_time_ms_) {
        LOG(log_deb_) << "CONSENSUSDBG round " << pbft_mgr_->round_
                      << " , step " << pbft_mgr_->state_
                      << " | have_next_voted_soft_value_ = "
                      << pbft_mgr_->have_next_voted_soft_value_
                      << " soft block = "
                      << pbft_mgr_->soft_voted_block_for_this_round_
                      << " have_next_voted_soft_value_ = "
                      << pbft_mgr_->have_next_voted_soft_value_
                      << " next_voted_block_from_previous_round_ = "
                      << pbft_mgr_->next_voted_block_from_previous_round_
                      << " cert voted = "
                      << (pbft_mgr_->cert_voted_values_for_round_.find(
                              pbft_mgr_->round_) !=
                          pbft_mgr_->cert_voted_values_for_round_.end());
        return (pbft_mgr_->hasChainedBlockFromRound()) ? vote_next_block_state
                                                       : chain_block_state;
      }
      return vote_next_block_polling_state;
    default:
      LOG(log_err_) << "Invalid PBFT state: " << pbft_mgr_->state_;
      return pbft_mgr_->state_;
  }
}
}  // namespace taraxa
