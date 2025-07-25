#include "pbft/proposed_blocks.hpp"

#include "pbft/pbft_block.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa {

bool ProposedBlocks::pushProposedPbftBlock(const std::shared_ptr<PbftBlock>& proposed_block, bool save_to_db) {
  std::unique_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(proposed_block->getPeriod());
  // Add period
  if (found_period_it == proposed_blocks_.end()) {
    found_period_it = proposed_blocks_.insert({proposed_block->getPeriod(), {}}).first;
  }

  if (save_to_db) {
    db_->saveProposedPbftBlock(proposed_block);
  }

  // Add propose vote & block
  return found_period_it->second.insert({proposed_block->getBlockHash(), {proposed_block, false}}).second;
}

void ProposedBlocks::markBlockAsValid(const std::shared_ptr<PbftBlock>& proposed_block) {
  std::unique_lock lock(proposed_blocks_mutex_);

  const auto found_period_it = proposed_blocks_.find(proposed_block->getPeriod());
  assert(found_period_it != proposed_blocks_.end());

  auto found_block_it = found_period_it->second.find(proposed_block->getBlockHash());
  // Set validation flag to true
  found_block_it->second.second = true;
}

std::optional<std::pair<std::shared_ptr<PbftBlock>, bool>> ProposedBlocks::getPbftProposedBlock(
    PbftPeriod period, const blk_hash_t& block_hash) const {
  std::shared_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(period);
  if (found_period_it == proposed_blocks_.end()) {
    return {};
  }

  auto found_block_it = found_period_it->second.find(block_hash);
  if (found_block_it == found_period_it->second.end()) {
    return {};
  }

  return found_block_it->second;
}

bool ProposedBlocks::isInProposedBlocks(PbftPeriod period, const blk_hash_t& block_hash) const {
  std::shared_lock lock(proposed_blocks_mutex_);

  auto found_period_it = proposed_blocks_.find(period);
  if (found_period_it == proposed_blocks_.end()) {
    return false;
  }

  return found_period_it->second.contains(block_hash);
}

void ProposedBlocks::cleanupProposedPbftBlocksByPeriod(PbftPeriod period) {
  std::unique_lock lock(proposed_blocks_mutex_);

  for (auto period_it = proposed_blocks_.begin(); period_it != proposed_blocks_.end() && period_it->first < period;) {
    auto batch = db_->createWriteBatch();
    for (const auto& block : period_it->second) {
      db_->removeProposedPbftBlock(block.first, batch);
    }
    db_->commitWriteBatch(batch);
    period_it = proposed_blocks_.erase(period_it);
  }
}

std::optional<std::string> ProposedBlocks::checkOldBlocksPresence(PbftPeriod current_period) const {
  std::string msg;
  for (auto period_it = proposed_blocks_.begin();
       period_it != proposed_blocks_.end() && period_it->first < current_period; period_it++) {
    msg += std::to_string(period_it->first) + " -> " + std::to_string(period_it->second.size()) + ". ";
  }

  return msg.empty() ? std::nullopt : std::make_optional(msg);
}

std::map<PbftPeriod, std::vector<std::shared_ptr<PbftBlock>>> ProposedBlocks::getProposedBlocks() const {
  std::map<PbftPeriod, std::vector<std::shared_ptr<PbftBlock>>> result;

  for (auto period_it = proposed_blocks_.begin(); period_it != proposed_blocks_.end(); period_it++) {
    for (auto block_it = period_it->second.begin(); block_it != period_it->second.end(); block_it++) {
      result[period_it->first].push_back(block_it->second.first);
    }
  }

  return result;
}

}  // namespace taraxa
