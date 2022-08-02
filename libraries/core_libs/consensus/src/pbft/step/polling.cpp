#include "pbft/step/polling.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Polling::init() { LOG(log_dg_) << "Will go to first finish State"; }

void Polling::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // Odd number steps from 5 are in second finish
  auto round = round_->getId();
  LOG(log_tr_) << "PBFT second finishing state at step " << id_ << " in round " << round;
  auto end_time_for_step = (id_ + 1) * round_->getLambda();
  LOG(log_tr_) << "Step " << id_ << " end time " << end_time_for_step;

  pm->updateSoftVotedBlockForThisRound_();
  if (round_->soft_voted_block_) {
    auto voted_block_hash_with_soft_votes =
        node_->vote_mgr_->getVotesBundleByRoundAndStep(round, 2, pm->TWO_T_PLUS_ONE);
    if (voted_block_hash_with_soft_votes) {
      // Have enough soft votes for a voting value
      auto net = node_->network_.lock();
      assert(net);  // Should never happen
      net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes(
          std::move(voted_block_hash_with_soft_votes->votes));
      LOG(log_dg_) << "Node has seen enough soft votes voted at " << voted_block_hash_with_soft_votes->voted_block_hash
                   << ", regossip soft votes. In round " << round << " step " << id_;
    }
  }

  // We only want to give up soft voted value IF:
  // 1) haven't cert voted it
  // 2) we are looking at value that was next voted in previous round
  // 3) we don't have the block or if have block it can't be cert voted (yet)
  bool giveUpSoftVotedBlockInSecondFinish = round_->last_cert_voted_value_ == kNullBlockHash &&
                                            pm->last_soft_voted_value_ == round_->previous_round_next_voted_value_ &&
                                            giveUpSoftVotedBlock_() &&
                                            !pm->compareBlocksAndRewardVotes_(round_->soft_voted_block_);
  giveUpSoftVotedBlock_() && !pm->compareBlocksAndRewardVotes_(round_->soft_voted_block_);
  if (!round_->next_voted_soft_value_ && round_->soft_voted_block_ && !giveUpSoftVotedBlockInSecondFinish) {
    auto place_votes = placeVote_(round_->soft_voted_block_, next_vote_type, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting " << round_->soft_voted_block_ << " for round " << round
                   << ", at step " << id_;

      node_->db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
      round_->next_voted_soft_value_ = true;
    }
  }

  if (!round_->next_voted_null_block_hash_ && round >= 2 &&
      (giveUpSoftVotedBlockInSecondFinish || giveUpNextVotedBlock_())) {
    auto place_votes = placeVote_(kNullBlockHash, next_vote_type, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step " << id_;

      node_->db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      round_->next_voted_null_block_hash_ = true;
    }
  }

  // if (id_ > MAX_STEPS && (id_ - MAX_STEPS - 2) % 100 == 0) {
  //   pm->syncPbftChainFromPeers_(exceeded_max_steps, kNullBlockHash);
  // }

  if (id_ > MAX_STEPS && (id_ - MAX_STEPS - 2) % 100 == 0 && !round_->next_votes_already_broadcasted_) {
    LOG(log_er_) << "Node " << pm->node_addr_ << " broadcast next votes for previous round. In round " << round
                 << " step " << id_;
    if (auto net = node_->network_.lock()) {
      net->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->broadcastPreviousRoundNextVotesBundle();
    }
    round_->next_votes_already_broadcasted_ = true;
  }

  if (round_->time_from_start_ms_ > end_time_for_step) {
    finish_();
  }
}

void Polling::finish_() {
  finished_ = true;

  auto round = round_->getId();
  LOG(log_dg_) << "CONSENSUS debug round " << round << " , step " << id_
               << " | next_voted_soft_value_ = " << round_->next_voted_soft_value_
               << " soft block = " << round_->soft_voted_block_ << " next_voted_null_block_hash_ = "
               << round_->next_voted_null_block_hash_
               //  << " state_->last_soft_voted_value_ = " << state_->last_soft_voted_value_
               << " round_->last_cert_voted_value_ = " << round_->last_cert_voted_value_
               << " round_->previous_round_next_voted_value_ = " << round_->previous_round_next_voted_value_;
  auto batch = node_->db_->createWriteBatch();
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  node_->db_->commitWriteBatch(batch);
  round_->next_voted_soft_value_ = false;
  round_->next_voted_null_block_hash_ = false;
  round_->polling_state_print_log_ = true;
}

}  // namespace taraxa::step