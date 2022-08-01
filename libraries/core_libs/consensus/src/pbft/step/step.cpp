#include "pbft/step/step.hpp"

#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {

bool Step::giveUpNextVotedBlock_() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return false;
  }

  auto round = pm->getPbftRound();

  if (state_->last_cert_voted_value_ != kNullBlockHash) {
    // Last cert voted value should equal to voted value
    if (pm->polling_state_print_log_) {
      LOG(log_nf_) << "In round " << round << " step " << id_ << ", last cert voted value is "
                   << state_->last_cert_voted_value_;
      pm->polling_state_print_log_ = false;
    }
    return false;
  }

  if (state_->previous_round_next_voted_value_ == kNullBlockHash) {
    // In round 1 also return here
    LOG(log_nf_) << "In round " << round << " step " << id_
                 << ", have received 2t+1 next votes for kNullBlockHash for previous round.";
    return true;
  } else if (node_->next_votes_manager_->haveEnoughVotesForNullBlockHash()) {
    LOG(log_nf_)
        << "In round " << round << " step " << id_
        << ", There are 2 voted values in previous round, and have received 2t+1 next votes for kNullBlockHash";
    return true;
  }

  if (node_->pbft_chain_->findPbftBlockInChain(state_->previous_round_next_voted_value_)) {
    LOG(log_nf_) << "In round " << round << " step " << id_
                 << ", find voted value in PBFT chain already. Give up voted value "
                 << state_->previous_round_next_voted_value_;
    return true;
  }

  auto pbft_block = pm->getUnfinalizedBlock_(state_->previous_round_next_voted_value_);
  if (pbft_block) {
    // Have a block, but is it valid?
    if (!node_->pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      // Received the block, but not valid
      return true;
    }
  } else {
    LOG(log_dg_) << "Cannot find PBFT block " << state_->previous_round_next_voted_value_
                 << " in both queue and DB, have not got yet";
  }

  return false;
}

size_t Step::placeVote_(const blk_hash_t &blockhash, PbftVoteTypes vote_type, uint64_t round) {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return false;
  }
  if (!pm->weighted_votes_count_) {
    // No delegation
    return 0;
  }

  auto vote = pm->generateVote(blockhash, vote_type, round, id_);
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

}  // namespace taraxa