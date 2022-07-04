#include "pbft/step/polling.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Polling::init() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  LOG(log_dg_) << "Will go to first finish State";
  state_->state_ = PbftStates::finish;
  pm->setPbftStep(id_);
  state_->next_step_time_ms_ = 4 * pm->LAMBDA_ms;
}

void Polling::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // Odd number steps from 5 are in second finish
  auto round = pm->getPbftRound();
  LOG(log_tr_) << "PBFT second finishing state at step " << id_ << " in round " << round;
  assert(id_ >= pm->startingStepInRound_);
  auto end_time_for_step = (2 + id_ - pm->startingStepInRound_) * pm->LAMBDA_ms - POLLING_INTERVAL_ms;
  LOG(log_tr_) << "Step " << id_ << " end time " << end_time_for_step;

  pm->updateSoftVotedBlockForThisRound_();
  if (state_->soft_voted_block_for_this_round_) {
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
  bool giveUpSoftVotedBlockInSecondFinish =
      state_->last_cert_voted_value_ == kNullBlockHash &&
      state_->last_soft_voted_value_ == state_->previous_round_next_voted_value_ && pm->giveUpSoftVotedBlock_() &&
      !pm->compareBlocksAndRewardVotes_(state_->soft_voted_block_for_this_round_);
  pm->giveUpSoftVotedBlock_() && !pm->compareBlocksAndRewardVotes_(state_->soft_voted_block_for_this_round_);
  if (!pm->next_voted_soft_value_ && state_->soft_voted_block_for_this_round_ && !giveUpSoftVotedBlockInSecondFinish) {
    auto place_votes = placeVote_(state_->soft_voted_block_for_this_round_, next_vote_type, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting " << state_->soft_voted_block_for_this_round_
                   << " for round " << round << ", at step " << id_;

      node_->db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
      pm->next_voted_soft_value_ = true;
    }
  }

  if (!pm->next_voted_null_block_hash_ && round >= 2 &&
      (giveUpSoftVotedBlockInSecondFinish || giveUpNextVotedBlock_())) {
    auto place_votes = placeVote_(kNullBlockHash, next_vote_type, round);
    if (place_votes) {
      LOG(log_nf_) << "Next votes " << place_votes << " voting NULL BLOCK for round " << round << ", at step " << id_;

      node_->db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      pm->next_voted_null_block_hash_ = true;
    }
  }

  if (id_ > MAX_STEPS && (id_ - MAX_STEPS - 2) % 100 == 0) {
    pm->syncPbftChainFromPeers_(exceeded_max_steps, kNullBlockHash);
  }

  if (id_ > MAX_STEPS && (id_ - MAX_STEPS - 2) % 100 == 0 && !alreadyBroadcasted_) {
    LOG(log_dg_) << "Node " << pm->node_addr_ << " broadcast next votes for previous round. In round " << round
                 << " step " << id_;
    if (auto net = node_->network_.lock()) {
      net->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->broadcastPreviousRoundNextVotesBundle();
    }
    alreadyBroadcasted_ = true;
  }

  if (state_->elapsed_time_in_round_ms_ > end_time_for_step) {
    finish_();
  }
}

void Polling::finish_() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  finished_ = true;

  auto round = pm->getPbftRound();
  LOG(log_dg_) << "CONSENSUS debug round " << round << " , step " << id_
               << " | next_voted_soft_value_ = " << pm->next_voted_soft_value_
               << " soft block = " << state_->soft_voted_block_for_this_round_
               << " next_voted_null_block_hash_ = " << pm->next_voted_null_block_hash_
               << " state_->last_soft_voted_value_ = " << state_->last_soft_voted_value_
               << " state_->last_cert_voted_value_ = " << state_->last_cert_voted_value_
               << " state_->previous_round_next_voted_value_ = " << state_->previous_round_next_voted_value_;
  state_->state_ = finish;
  auto batch = node_->db_->createWriteBatch();
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  node_->db_->commitWriteBatch(batch);
  pm->next_voted_soft_value_ = false;
  pm->next_voted_null_block_hash_ = false;
  pm->polling_state_print_log_ = true;
  assert(id_ >= pm->startingStepInRound_);
  state_->next_step_time_ms_ += POLLING_INTERVAL_ms;
}

}  // namespace taraxa::step