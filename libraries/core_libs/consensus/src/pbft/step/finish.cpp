#include "pbft/step/finish.hpp"

#include <cassert>

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Finish::init() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  LOG(log_dg_) << "Will go to first finish State";
  state_->state_ = finish;
  pm->setPbftStep(id_);
  state_->next_step_time_ms_ = 4 * pm->LAMBDA_ms;
}

void Finish::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // Even number steps from 4 are in first finish
  auto round = pm->getPbftRound();
  LOG(log_tr_) << "PBFT first finishing state at step " << id_ << " in round " << round;

  if (state_->last_cert_voted_value_ != kNullBlockHash) {
    auto place_votes = placeVote_(state_->last_cert_voted_value_, next_vote_type, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting cert voted value " << state_->last_cert_voted_value_
                   << " for round " << round << " , step " << id_;
    }
    // Re-broadcast pbft block in case some nodes do not have it
    if (id_ % 20 == 0) {
      auto pbft_block = node_->db_->getPbftCertVotedBlock(state_->last_cert_voted_value_);
      assert(pbft_block);
      if (auto net = node_->network_.lock()) {
        net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
      }
    }
  } else {
    // We only want to give up soft voted value IF:
    // 1) haven't cert voted it
    // 2) we are looking at value that was next voted in previous round
    // 3) we don't have the block or if have block it can't be cert voted (yet)
    bool giveUpSoftVotedBlockInFirstFinish =
        state_->last_cert_voted_value_ == kNullBlockHash &&
        state_->own_starting_value_for_round_ == state_->previous_round_next_voted_value_ &&
        pm->giveUpSoftVotedBlock_() && !pm->compareBlocksAndRewardVotes_(state_->own_starting_value_for_round_);

    if (round >= 2 && (giveUpNextVotedBlock_() || giveUpSoftVotedBlockInFirstFinish)) {
      auto place_votes = placeVote_(kNullBlockHash, next_vote_type, round);
      if (place_votes) {
        LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step " << id_;
      }
    } else {
      if (state_->own_starting_value_for_round_ != state_->previous_round_next_voted_value_ &&
          state_->previous_round_next_voted_value_ != kNullBlockHash &&
          !node_->pbft_chain_->findPbftBlockInChain(state_->previous_round_next_voted_value_)) {
        if (state_->own_starting_value_for_round_ == kNullBlockHash) {
          node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound,
                                            state_->previous_round_next_voted_value_);
          state_->own_starting_value_for_round_ = state_->previous_round_next_voted_value_;
          LOG(log_dg_) << "Updating own starting value of NULL BLOCK HASH to previous round next voted value of "
                       << state_->previous_round_next_voted_value_;
        } else if (pm->compareBlocksAndRewardVotes_(state_->previous_round_next_voted_value_)) {
          // Check if we have received the previous round next voted value and its a viable value...
          // IF it is viable then reset own starting value to it...
          node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound,
                                            state_->previous_round_next_voted_value_);
          LOG(log_dg_) << "Updating own starting value of " << state_->own_starting_value_for_round_
                       << " to previous round next voted value of " << state_->previous_round_next_voted_value_;
          state_->own_starting_value_for_round_ = state_->previous_round_next_voted_value_;
        }
      }

      auto place_votes = placeVote_(state_->own_starting_value_for_round_, next_vote_type, round);
      if (place_votes) {
        LOG(log_nf_) << "Next votes " << place_votes << " voting nodes own starting value "
                     << state_->own_starting_value_for_round_ << " for round " << round << ", at step " << id_;
        // Re-broadcast pbft block in case some nodes do not have it
        if (id_ % 20 == 0) {
          auto pbft_block = pm->getUnfinalizedBlock_(state_->own_starting_value_for_round_);
          if (auto net = node_->network_.lock(); net && pbft_block) {
            net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
          }
        }
      }
    }
  }
}

}  // namespace taraxa::step