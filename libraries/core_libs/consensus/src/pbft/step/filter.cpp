#include "pbft/step/filter.hpp"

#include "dag/dag.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Filter::init() {}

void Filter::run() {
  // The Filtering Step
  auto round = round_->getId();
  LOG(log_tr_) << "PBFT filtering state in round " << round;

  if (giveUpNextVotedBlock_()) {
    // Identity leader
    auto leader_block = identifyLeaderBlock_();
    if (leader_block) {
      node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, leader_block);
      round_->own_starting_value_for_round_ = leader_block;
      LOG(log_dg_) << "Identify leader block " << leader_block << " at round " << round;

      auto place_votes = placeVote_(leader_block, soft_vote_type, round);

      updateLastSoftVotedValue_(leader_block);

      if (place_votes) {
        LOG(log_nf_) << "Soft votes " << place_votes << " voting block " << leader_block << " at round " << round;
      }
    }
  } else if (round_->previous_round_next_voted_value_) {
    auto place_votes = placeVote_(round_->previous_round_next_voted_value_, PbftVoteTypes::soft_vote_type, round);

    // Generally this value will either be the same as last soft voted value from previous round
    // but a node could have observed a previous round next voted value that differs from what they
    // soft voted.
    updateLastSoftVotedValue_(round_->previous_round_next_voted_value_);

    if (place_votes) {
      LOG(log_nf_) << "Soft votes " << place_votes << " voting block " << round_->previous_round_next_voted_value_
                   << " from previous round. In round " << round;
    }
  }

  finish_();
}

blk_hash_t Filter::identifyLeaderBlock_() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return {};
  }

  auto round_id = round_->getId();
  LOG(log_dg_) << "Into identify leader block, in round " << round_id;

  // Get all proposal votes in the round
  auto votes = node_->vote_mgr_->getProposalVotes(round_id);

  // Each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<h256, blk_hash_t>> leader_candidates;

  for (auto const &v : votes) {
    if (v->getRound() != round_id) {
      LOG(log_er_) << "Vote round is not same with current round " << round_id << ". Vote " << v;
      continue;
    }
    if (v->getStep() != 1) {
      LOG(log_er_) << "Vote step is not 1. Vote " << v;
      continue;
    }
    if (v->getType() != propose_vote_type) {
      LOG(log_er_) << "Vote type is not propose vote type. Vote " << v;
      continue;
    }

    const auto proposed_block_hash = v->getBlockHash();
    if (proposed_block_hash == kNullBlockHash) {
      LOG(log_er_) << "Propose block hash should not be NULL. Vote " << v;
      continue;
    }
    if (node_->pbft_chain_->findPbftBlockInChain(proposed_block_hash)) {
      continue;
    }
    // Make sure we don't keep soft voting for soft value we want to give up...
    if (proposed_block_hash == pm->last_soft_voted_value_ && giveUpSoftVotedBlock_()) {
      continue;
    }

    leader_candidates.emplace_back(std::make_pair(getProposal(v), proposed_block_hash));
  }

  if (leader_candidates.empty()) {
    // no eligible leader
    return kNullBlockHash;
  }

  const auto leader = *std::min_element(leader_candidates.begin(), leader_candidates.end(),
                                        [](const auto &i, const auto &j) { return i.first < j.first; });
  return leader.second;
}

void Filter::updateLastSoftVotedValue_(blk_hash_t const new_soft_voted_value) {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }

  if (new_soft_voted_value != pm->last_soft_voted_value_) {
    round_->time_began_waiting_soft_voted_block_ = std::chrono::system_clock::now();
  }
  pm->last_soft_voted_value_ = new_soft_voted_value;
}

h256 Filter::getProposal(const std::shared_ptr<Vote> &vote) const {
  auto lowest_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), 1);
  for (uint64_t i = 2; i <= vote->getWeight(); ++i) {
    auto tmp_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), i);
    if (lowest_hash > tmp_hash) {
      lowest_hash = tmp_hash;
    }
  }
  return lowest_hash;
}

}  // namespace taraxa::step