#include "pbft/proposed_blocks.hpp"

#include "pbft/pbft_block.hpp"
#include "vote/vote.hpp"

namespace taraxa {

bool ProposedBlocks::pushProposedPbftBlock(const std::shared_ptr<PbftBlock>& proposed_block,
                                           const std::shared_ptr<Vote>& propose_vote) {
  assert(propose_vote->getBlockHash() == proposed_block->getBlockHash());
  assert(propose_vote->getPeriod() == proposed_block->getPeriod());
  return pushProposedPbftBlock(propose_vote->getRound(), proposed_block);
}

bool ProposedBlocks::pushProposedPbftBlock(uint64_t round, const std::shared_ptr<PbftBlock>& proposed_block,
                                           bool save_to_db) {
  std::unique_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(proposed_block->getPeriod());
  // Add period
  if (found_period_it == proposed_blocks_.end()) {
    found_period_it = proposed_blocks_.insert({proposed_block->getPeriod(), {}}).first;
  }

  auto found_round_it = found_period_it->second.find(round);
  // Add round
  if (found_round_it == found_period_it->second.end()) {
    found_round_it = found_period_it->second.insert({round, {}}).first;
  }

  if (save_to_db) {
    db_->saveProposedPbftBlock(proposed_block, round);
  }

  // Add propose vote & block
  return found_round_it->second.insert({proposed_block->getBlockHash(), {proposed_block, false}}).second;
}

void ProposedBlocks::markBlockAsValid(uint64_t round, const std::shared_ptr<PbftBlock>& proposed_block) {
  std::unique_lock lock(proposed_blocks_mutex_);

  const auto found_period_it = proposed_blocks_.find(proposed_block->getPeriod());
  assert(found_period_it != proposed_blocks_.end());

  const auto found_round_it = found_period_it->second.find(round);
  assert(found_round_it != found_period_it->second.end());

  auto found_block_it = found_round_it->second.find(proposed_block->getBlockHash());
  // Set validation flag to true
  found_block_it->second.second = true;
}

std::optional<std::pair<std::shared_ptr<PbftBlock>, bool>> ProposedBlocks::getPbftProposedBlock(
    uint64_t period, uint64_t round, const blk_hash_t& block_hash) const {
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

  return found_block_it->second;
}

bool ProposedBlocks::isInProposedBlocks(uint64_t period, uint64_t round, const blk_hash_t& block_hash) const {
  std::shared_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(period);
  if (found_period_it == proposed_blocks_.end()) {
    return false;
  }

  auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return false;
  }

  return found_round_it->second.contains(block_hash);
}

void ProposedBlocks::cleanupProposedPbftBlocksByPeriod(uint64_t period) {
  std::unique_lock lock(proposed_blocks_mutex_);

  for (auto period_it = proposed_blocks_.begin(); period_it != proposed_blocks_.end() && period_it->first < period;) {
    auto batch = db_->createWriteBatch();
    for (auto round : period_it->second) {
      for (const auto& block : round.second) {
        db_->removeProposedPbftBlock(block.first, batch);
      }
    }
    db_->commitWriteBatch(batch);
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
       round_it != found_period_it->second.end() && round_it->first < round - 1;) {
    auto batch = db_->createWriteBatch();
    for (const auto& block : round_it->second) {
      db_->removeProposedPbftBlock(block.first, batch);
    }
    db_->commitWriteBatch(batch);
    round_it = found_period_it->second.erase(round_it);
  }
}

}  // namespace taraxa