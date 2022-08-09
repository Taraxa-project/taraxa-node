#include "pbft/step/step.hpp"

#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {

Step::Step(uint64_t id, const std::chrono::milliseconds& finish_time, std::shared_ptr<RoundFace> round)
    : kId_(id),
      kType_(stepTypeFromId(id)),
      kFinishTime_(finish_time),
      round_(std::move(round)),
      node_(round_->getNode()),
      LOG_OBJECTS_INITIALIZE_FROM_SHARED(node_->step_logger) {}

bool Step::giveUpNextVotedBlock() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return false;
  }

  auto round = round_->getId();

  if (round_->last_cert_voted_value_ != kNullBlockHash) {
    // Last cert voted value should equal to voted value
    if (round_->polling_state_print_log_) {
      LOG(log_nf_) << "In round " << round << " step " << kId_ << ", last cert voted value is "
                   << round_->last_cert_voted_value_;
      round_->polling_state_print_log_ = false;
    }
    return false;
  }

  if (round_->previous_round_next_voted_value_ == kNullBlockHash) {
    // In round 1 also return here
    LOG(log_nf_) << "In round " << round << " step " << kId_
                 << ", have received 2t+1 next votes for kNullBlockHash for previous round.";
    return true;
  } else if (round_->previous_round_next_voted_null_block_hash_) {
    LOG(log_nf_)
        << "In round " << round << " step " << kId_
        << ", There are 2 voted values in previous round, and have received 2t+1 next votes for kNullBlockHash";
    return true;
  }

  if (node_->pbft_chain_->findPbftBlockInChain(round_->previous_round_next_voted_value_)) {
    LOG(log_nf_) << "In round " << round << " step " << kId_
                 << ", find voted value in PBFT chain already. Give up voted value "
                 << round_->previous_round_next_voted_value_;
    return true;
  }

  auto pbft_block = pm->getUnfinalizedBlock(round_->previous_round_next_voted_value_);
  if (pbft_block) {
    // Have a block, but is it valid?
    if (!node_->pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      // Received the block, but not valid
      return true;
    }
  } else {
    LOG(log_dg_) << "Cannot find PBFT block " << round_->previous_round_next_voted_value_
                 << " in both queue and DB, have not got yet";
  }

  return false;
}

PbftVoteType Step::getVoteType() const {
  if (kId_ > StepType::certify) {
    return PbftVoteType::next_vote_type;
  }
  return PbftVoteType(kId_ - 1);
}

size_t Step::placeVote(const blk_hash_t& blockhash, uint64_t round) {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return false;
  }
  if (!pm->weighted_votes_count_) {
    // No delegation
    return 0;
  }
  auto vote_type = getVoteType();
  auto vote = pm->generateVote(blockhash, vote_type, round, kId_);
  const auto weight =
      vote->calculateWeight(pm->getDposWeightedVotesCount(), pm->getDposTotalVotesCount(), pm->getThreshold(vote_type));
  if (weight) {
    node_->db_->saveVerifiedVote(vote);
    node_->vote_mgr_->addVerifiedVote(vote);
    if (auto net = node_->network_.lock()) {
      net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes({std::move(vote)});
    }
  }

  return weight;
}

bool Step::giveUpSoftVotedBlock() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return false;
  }

  if (pm->last_soft_voted_value_ == kNullBlockHash) return false;

  auto now = std::chrono::system_clock::now();
  auto soft_voted_block_wait_duration = now - round_->time_began_waiting_soft_voted_block_;
  unsigned long elapsed_wait_soft_voted_block_in_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(soft_voted_block_wait_duration).count();

  auto pbft_block = pm->getUnfinalizedBlock(round_->previous_round_next_voted_value_);
  if (pbft_block) {
    // Have a block, but is it valid?
    if (!node_->pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      // Received the block, but not valid
      return true;
    }
  }

  if (elapsed_wait_soft_voted_block_in_ms > pm->max_wait_for_soft_voted_block_steps_ms_) {
    LOG(log_dg_) << "Have been waiting " << elapsed_wait_soft_voted_block_in_ms << "ms for soft voted block "
                 << pm->last_soft_voted_value_ << ", giving up on this value.";
    return true;
  } else {
    LOG(log_tr_) << "Have only been waiting " << elapsed_wait_soft_voted_block_in_ms << "ms for soft voted block "
                 << pm->last_soft_voted_value_ << "(after " << pm->max_wait_for_soft_voted_block_steps_ms_
                 << "ms will give up on this value)";
  }

  return false;
}

}  // namespace taraxa