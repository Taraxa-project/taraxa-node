#include "rewards/rewards_stats.hpp"

#include "storage/storage.hpp"

namespace taraxa::rewards {
Stats::Stats(uint32_t committee_size, const Hardforks& hardforks, std::shared_ptr<DB> db,
             std::function<uint64_t(EthBlockNumber)>&& dpos_eligible_total_vote_count)
    : kCommitteeSize(committee_size),
      kHardforks(hardforks),
      db_(std::move(db)),
      dpos_eligible_total_vote_count_(dpos_eligible_total_vote_count) {
  loadFromDb();
}

void Stats::loadFromDb() {
  auto i = db_->getColumnIterator(DB::Columns::block_rewards_stats);
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    blocks_stats_.push_back(util::rlp_dec<BlockStats>(dev::RLP(i->value().ToString())));
  }
}

void Stats::saveBlockStats(uint64_t period, const BlockStats& stats) {
  dev::RLPStream encoding;
  stats.rlp(encoding);

  db_->insert(DB::Columns::block_rewards_stats, period, encoding.out());
}

uint32_t Stats::getCurrentDistributionFrequency(uint64_t current_block) const {
  auto distribution_frequencies = kHardforks.rewards_distribution_frequency;
  auto itr = distribution_frequencies.upper_bound(current_block);
  if (distribution_frequencies.empty() || itr == distribution_frequencies.begin()) {
    return 1;
  }
  return (--itr)->second;
}

void Stats::clear() {
  // clear need to be called on vector because it was moved before
  blocks_stats_.clear();
  db_->deleteColumnData(DB::Columns::block_rewards_stats);
}

BlockStats Stats::getBlockStats(const PeriodData& blk, std::vector<gas_t> trxs_fees) {
  uint64_t dpos_vote_count = kCommitteeSize;

  // Block zero
  if (!blk.previous_block_cert_votes.empty()) [[likely]] {
    dpos_vote_count = dpos_eligible_total_vote_count_(blk.previous_block_cert_votes[0]->getPeriod() - 1);
  }
  if (blk.pbft_blk->getPeriod() < kHardforks.magnolia_hf_block_num) {
    trxs_fees = {};
  }

  return BlockStats{blk, trxs_fees, dpos_vote_count, kCommitteeSize};
}

std::vector<BlockStats> Stats::processStats(const PeriodData& current_blk, const std::vector<gas_t>& trxs_gas_used) {
  const auto current_period = current_blk.pbft_blk->getPeriod();
  const auto frequency = getCurrentDistributionFrequency(current_period);
  auto block_stats = getBlockStats(current_blk, trxs_gas_used);
  // Distribute rewards every block
  if (frequency == 1) {
    return {block_stats};
  }

  blocks_stats_.push_back(std::move(block_stats));
  // Blocks between distribution. Process and save for future processing
  if (current_period % frequency != 0) {
    // Save to db, so in case of restart data could be just loaded for the period
    saveBlockStats(current_period, *blocks_stats_.rbegin());
    return {};
  }

  std::vector<BlockStats> res(std::move(blocks_stats_));
  clear();
  return res;
}

}  // namespace taraxa::rewards