#include "pbft/step/polling.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Polling::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // Odd number steps from 5 are in second finish
  auto round = round_->getId();
  LOG(log_tr_) << "PBFT second finishing state at step " << kId_ << " in round " << round;
  LOG(log_tr_) << "Step " << kId_ << " end time " << kFinishTime_.count();

  pm->updateSoftVotedBlockForThisRound();
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
                   << ", regossip soft votes. In round " << round << " step " << kId_;
    }
  }

  // We only want to give up soft voted value IF:
  // 1) haven't cert voted it
  // 2) we are looking at value that was next voted in previous round
  // 3) we don't have the block or if have block it can't be cert voted (yet)
  bool giveUpSoftVotedBlockInSecondFinish = round_->last_cert_voted_value_ == kNullBlockHash &&
                                            pm->last_soft_voted_value_ == round_->previous_round_next_voted_value_ &&
                                            giveUpSoftVotedBlock() &&
                                            !pm->compareBlocksAndRewardVotes(round_->soft_voted_block_);
  giveUpSoftVotedBlock() && !pm->compareBlocksAndRewardVotes(round_->soft_voted_block_);
  if (!round_->next_voted_soft_value_ && round_->soft_voted_block_ && !giveUpSoftVotedBlockInSecondFinish) {
    auto place_votes = placeVote(round_->soft_voted_block_, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting " << round_->soft_voted_block_ << " for round " << round
                   << ", at step " << kId_;

      node_->db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
      round_->next_voted_soft_value_ = true;
    }
  }

  if (!round_->next_voted_null_block_hash_ && round >= 2 &&
      (giveUpSoftVotedBlockInSecondFinish || giveUpNextVotedBlock())) {
    auto place_votes = placeVote(kNullBlockHash, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step " << kId_;

      node_->db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      round_->next_voted_null_block_hash_ = true;
    }
  }

  // if (kId_ > MAX_STEPS && (kId_ - MAX_STEPS - 2) % 100 == 0) {
  //   pm->syncPbftChainFromPeers_(exceeded_max_steps, kNullBlockHash);
  // }

  if (kId_ > MAX_STEPS && (kId_ - MAX_STEPS - 2) % 100 == 0 && !round_->next_votes_already_broadcasted_) {
    LOG(log_er_) << "Node " << pm->node_addr_ << " broadcast next votes for previous round. In round " << round
                 << " step " << kId_;
    if (auto net = node_->network_.lock()) {
      net->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->broadcastPreviousRoundNextVotesBundle();
    }
    round_->next_votes_already_broadcasted_ = true;
  }

  if (round_->time_from_start_ms_ > kFinishTime_) {
    finish();
  }
}

void Polling::finish() {
  Step::finish();
  auto round = round_->getId();
  LOG(log_dg_) << "CONSENSUS debug round " << round << " , step " << kId_
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