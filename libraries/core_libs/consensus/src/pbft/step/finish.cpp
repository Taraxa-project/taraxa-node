#include "pbft/step/finish.hpp"

#include <cassert>

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Finish::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // Even number steps from 4 are in first finish
  auto round = round_->getId();
  LOG(log_tr_) << "PBFT first finishing state at step " << kId_ << " in round " << round;

  if (round_->last_cert_voted_value_ != kNullBlockHash) {
    auto place_votes = placeVote(round_->last_cert_voted_value_, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting cert voted value " << round_->last_cert_voted_value_
                   << " for round " << round << " , step " << kId_;
    }
    // Re-broadcast pbft block in case some nodes do not have it
    if (kId_ % 20 == 0) {
      auto pbft_block = node_->db_->getPbftCertVotedBlock(round_->last_cert_voted_value_);
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
        round_->last_cert_voted_value_ == kNullBlockHash &&
        round_->own_starting_value_for_round_ == round_->previous_round_next_voted_value_ && giveUpSoftVotedBlock() &&
        !pm->compareBlocksAndRewardVotes(round_->own_starting_value_for_round_);

    if (round >= 2 && (giveUpNextVotedBlock() || giveUpSoftVotedBlockInFirstFinish)) {
      auto place_votes = placeVote(kNullBlockHash, round);
      if (place_votes) {
        LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step "
                     << kId_;
      }
    } else {
      if (round_->own_starting_value_for_round_ != round_->previous_round_next_voted_value_ &&
          round_->previous_round_next_voted_value_ != kNullBlockHash &&
          !node_->pbft_chain_->findPbftBlockInChain(round_->previous_round_next_voted_value_)) {
        if (round_->own_starting_value_for_round_ == kNullBlockHash) {
          node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound,
                                            round_->previous_round_next_voted_value_);
          round_->own_starting_value_for_round_ = round_->previous_round_next_voted_value_;
          LOG(log_dg_) << "Updating own starting value of NULL BLOCK HASH to previous round next voted value of "
                       << round_->previous_round_next_voted_value_;
        } else if (pm->compareBlocksAndRewardVotes(round_->previous_round_next_voted_value_)) {
          // Check if we have received the previous round next voted value and its a viable value...
          // IF it is viable then reset own starting value to it...
          node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound,
                                            round_->previous_round_next_voted_value_);
          LOG(log_dg_) << "Updating own starting value of " << round_->own_starting_value_for_round_
                       << " to previous round next voted value of " << round_->previous_round_next_voted_value_;
          round_->own_starting_value_for_round_ = round_->previous_round_next_voted_value_;
        }
      }

      auto place_votes = placeVote(round_->own_starting_value_for_round_, round);
      if (place_votes) {
        LOG(log_nf_) << "Next votes " << place_votes << " voting nodes own starting value "
                     << round_->own_starting_value_for_round_ << " for round " << round << ", at step " << kId_;
        // Re-broadcast pbft block in case some nodes do not have it
        if (kId_ % 20 == 0) {
          auto pbft_block = pm->getUnfinalizedBlock(round_->own_starting_value_for_round_);
          if (auto net = node_->network_.lock(); net && pbft_block) {
            net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
          }
        }
      }
    }
  }
  finish();
}

}  // namespace taraxa::step