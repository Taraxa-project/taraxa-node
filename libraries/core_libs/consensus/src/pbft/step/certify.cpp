#include "pbft/step/certify.hpp"

#include "dag/dag.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Certify::init() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  state_->state_ = certify;
  pm->setPbftStep(id_);
  state_->next_step_time_ms_ = 2 * pm->LAMBDA_ms;
}

void Certify::run() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // The Certifying Step
  auto round = pm->getPbftRound();
  LOG(log_tr_) << "PBFT certifying state in round " << round;

  // Should be moved to FSM step registration
  finished_ = state_->elapsed_time_in_round_ms_ > 4 * pm->LAMBDA_ms - POLLING_INTERVAL_ms;
  if (state_->elapsed_time_in_round_ms_ < 2 * pm->LAMBDA_ms) {
    // Should not happen, add log here for safety checking
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only " << state_->elapsed_time_in_round_ms_
                 << " (ms) in round " << round;
    return;
  }

  auto step_timeout = 4 * pm->LAMBDA_ms - POLLING_INTERVAL_ms;

  finished_ = state_->elapsed_time_in_round_ms_ > step_timeout;

  if (state_->elapsed_time_in_round_ms_ > step_timeout) {
    finish_();
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in round " << round;
    return;
  }

  // remove this check after proper step switching will be done
  if (should_have_cert_voted_in_this_round_) {
    return;
  }
  LOG(log_tr_) << "In step 3";

  if (pm->updateSoftVotedBlockForThisRound_() &&
      pm->compareBlocksAndRewardVotes_(state_->soft_voted_block_for_this_round_)) {
    LOG(log_tr_) << "Finished compareBlocksAndRewardVotes_";

    // NOTE: If we have already executed this round then block won't be found in unverified queue...
    bool executed_soft_voted_block_for_this_round = false;
    if (pm->have_executed_this_round_) {
      LOG(log_tr_) << "Have already executed before certifying in step 3 in round " << round;
      auto last_pbft_block_hash = node_->pbft_chain_->getLastPbftBlockHash();
      if (last_pbft_block_hash == state_->soft_voted_block_for_this_round_) {
        LOG(log_tr_) << "Having executed, last block in chain is the soft voted block in round " << round;
        executed_soft_voted_block_for_this_round = true;
      }
    }

    bool unverified_soft_vote_block_for_this_round_is_valid = false;
    if (!executed_soft_voted_block_for_this_round) {
      auto block = node_->pbft_chain_->getUnverifiedPbftBlock(state_->soft_voted_block_for_this_round_);
      if (block && node_->pbft_chain_->checkPbftBlockValidation(*block)) {
        unverified_soft_vote_block_for_this_round_is_valid = true;
      } else {
        if (!block) {
          LOG(log_er_) << "Cannot find the unverified pbft block " << state_->soft_voted_block_for_this_round_
                       << " in round " << round << " step 3";
        }
        pm->syncPbftChainFromPeers_(invalid_soft_voted_block, state_->soft_voted_block_for_this_round_);
      }
    }

    if (executed_soft_voted_block_for_this_round || unverified_soft_vote_block_for_this_round_is_valid) {
      // compareBlocksAndRewardVotes_ has checked the cert voted block exist
      state_->last_cert_voted_value_ = state_->soft_voted_block_for_this_round_;
      auto cert_voted_block = pm->getUnfinalizedBlock_(state_->last_cert_voted_value_);

      auto batch = node_->db_->createWriteBatch();
      node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::LastCertVotedValue, state_->last_cert_voted_value_,
                                              batch);
      node_->db_->addPbftCertVotedBlockToBatch(*cert_voted_block, batch);
      node_->db_->commitWriteBatch(batch);

      should_have_cert_voted_in_this_round_ = true;

      // generate cert vote
      auto place_votes = placeVote_(state_->soft_voted_block_for_this_round_, cert_vote_type, round);
      if (place_votes) {
        LOG(log_nf_) << "Cert votes " << place_votes << " voting " << state_->soft_voted_block_for_this_round_
                     << " in round " << round;
      }
    }
  }
}

}  // namespace taraxa::step