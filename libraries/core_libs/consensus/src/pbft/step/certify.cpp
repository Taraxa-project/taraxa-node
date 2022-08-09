#include "pbft/step/certify.hpp"

#include "dag/dag.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Certify::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // The Certifying Step
  auto round = round_->getId();
  LOG(log_tr_) << "PBFT certifying state in round " << round;

  // Should be moved to FSM step registration
  if (round_->time_from_start_ms_ < 2 * round_->getLambda()) {
    // Should not happen, add log here for safety checking
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only " << round_->time_from_start_ms_.count()
                 << " (ms) in round " << round;
    return;
  }

  auto step_timeout = 4 * round_->getLambda();
  if (round_->time_from_start_ms_ > step_timeout) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in round " << round;
    finish();
    return;
  }

  // remove this check after proper step switching will be done

  if (pm->updateSoftVotedBlockForThisRound() && pm->compareBlocksAndRewardVotes(round_->soft_voted_block_)) {
    LOG(log_tr_) << "Finished compareBlocksAndRewardVotes";

    // NOTE: If we have already executed this round then block won't be found in unverified queue...
    bool executed_soft_voted_block_for_this_round = false;
    if (round_->block_certified_) {
      LOG(log_tr_) << "Have already executed before certifying in step 3 in round " << round;
      auto last_pbft_block_hash = node_->pbft_chain_->getLastPbftBlockHash();
      if (last_pbft_block_hash == round_->soft_voted_block_) {
        LOG(log_tr_) << "Having executed, last block in chain is the soft voted block in round " << round;
        executed_soft_voted_block_for_this_round = true;
      }
    }

    bool unverified_soft_vote_block_for_this_round_is_valid = false;
    if (!executed_soft_voted_block_for_this_round) {
      auto block = node_->pbft_chain_->getUnverifiedPbftBlock(round_->soft_voted_block_);
      if (block && node_->pbft_chain_->checkPbftBlockValidation(*block)) {
        unverified_soft_vote_block_for_this_round_is_valid = true;
      } else {
        if (!block) {
          LOG(log_er_) << "Cannot find the unverified pbft block " << round_->soft_voted_block_ << " in round " << round
                       << " step 3";
        }
        // pm->syncPbftChainFromPeers_(invalid_soft_voted_block, round_->soft_voted_block_);
      }
    }

    if (executed_soft_voted_block_for_this_round || unverified_soft_vote_block_for_this_round_is_valid) {
      // compareBlocksAndRewardVotes has checked the cert voted block exist
      round_->last_cert_voted_value_ = round_->soft_voted_block_;
      auto cert_voted_block = pm->getUnfinalizedBlock(round_->last_cert_voted_value_);

      auto batch = node_->db_->createWriteBatch();
      node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::LastCertVotedValue, round_->last_cert_voted_value_,
                                              batch);
      node_->db_->addPbftCertVotedBlockToBatch(*cert_voted_block, batch);
      node_->db_->commitWriteBatch(batch);

      // generate cert vote
      auto place_votes = placeVote(round_->soft_voted_block_, round);
      if (place_votes) {
        LOG(log_nf_) << "Cert votes " << place_votes << " voting " << round_->soft_voted_block_ << " in round "
                     << round;
      }
      finish();
    }
  }
}

}  // namespace taraxa::step