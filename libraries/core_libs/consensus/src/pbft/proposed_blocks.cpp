#include "pbft/proposed_blocks.hpp"
#include "pbft/pbft_block.hpp"
#include "vote/vote.hpp"

namespace taraxa {

std::pair<bool, std::string> ProposedBlocks::pushProposedPbftBlock(const std::shared_ptr<PbftBlock>& proposed_block, const std::shared_ptr<Vote>& propose_vote) {
  if (propose_vote->getBlockHash() != proposed_block->getBlockHash()) {
    return {false, "Propose vote block hash " + propose_vote->getBlockHash().abridged() + " != actual block hash " + proposed_block->getBlockHash().abridged()};
  }

  std::unique_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(propose_vote->getPeriod());
  // Add period
  if (found_period_it == proposed_blocks_.end()) {
    found_period_it = proposed_blocks_.insert({propose_vote->getPeriod(), {}}).first;
  }

  auto found_round_it = found_period_it->second.find(propose_vote->getRound());
  // Add round
  if (found_round_it == found_period_it->second.end()) {
    found_round_it = found_period_it->second.insert({propose_vote->getRound(), {}}).first;
  }

  // Add propose vote & block
  if (!found_round_it->second.insert({proposed_block->getBlockHash(), std::make_pair(propose_vote->getHash(), proposed_block)}).second) {
    return {false, "Proposed block " + proposed_block->getBlockHash().abridged() + " already inserted(race condition)"};
  }

  return {true, ""};
}

std::shared_ptr<PbftBlock> ProposedBlocks::getPbftProposedBlock(uint64_t period, uint64_t round, const blk_hash_t& block_hash) const {
  std::shared_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(period);
  if (found_period_it == proposed_blocks_.end()) {
    return {};
  }

  auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  auto found_block_it = found_round_it->second.find(block_hash);
  if (found_block_it == found_round_it->second.end()) {
    return {};
  }

  return found_block_it->second.second;
}

void ProposedBlocks::cleanupProposedPbftBlocksByPeriod(uint64_t period) {
  std::unique_lock lock(proposed_blocks_mutex_);

  for (auto period_it = proposed_blocks_.begin();
       period_it != proposed_blocks_.end() && period_it->first < period; period_it++) {
    period_it = proposed_blocks_.erase(period_it);
  }
}

void ProposedBlocks::cleanupProposedPbftBlocksByRound(uint64_t period, uint64_t round) {
  // We must keep previous round proposed blocks for voting purposes
  if (round < 3) {
    return;
  }

  std::unique_lock lock(proposed_blocks_mutex_);
  auto found_period_it = proposed_blocks_.find(period);

  if (found_period_it == proposed_blocks_.end()) {
    return;
  }

  for (auto round_it = found_period_it->second.begin();
       round_it != found_period_it->second.end() && round_it->first < round - 1; round_it++) {
    round_it = found_period_it->second.erase(round_it);
  }
}

}  // namespace taraxa