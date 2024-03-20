#include "rewards/rewards_stats.hpp"

#include "storage/storage.hpp"

namespace taraxa::rewards {
Stats::Stats(uint32_t committee_size, const HardforksConfig& hardforks, std::shared_ptr<DB> db,
             std::function<uint64_t(EthBlockNumber)>&& dpos_eligible_total_vote_count,
             std::optional<EthBlockNumber> last_blk_num)
    : kCommitteeSize(committee_size),
      kHardforksConfig(hardforks),
      db_(std::move(db)),
      dpos_eligible_total_vote_count_(dpos_eligible_total_vote_count) {
  recoverFromDb(last_blk_num);
}

void Stats::recoverFromDb(std::optional<EthBlockNumber> last_blk_num) {
  auto i = db_->getColumnIterator(DB::Columns::block_rewards_stats);
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    PbftPeriod period;
    memcpy(&period, i->key().data(), sizeof(PbftPeriod));
    if (last_blk_num && *last_blk_num >= period) {
      std::cout << "Skipping block rewards stats recovery for period " << period << " as it is already processed"
                << std::endl;
      continue;
    }
    blocks_stats_[period] = util::rlp_dec<BlockStats>(dev::RLP(i->value().ToString()));
  }
}

void Stats::saveBlockStats(uint64_t period, const BlockStats& stats, DbStorage::Batch& write_batch) {
  dev::RLPStream encoding;
  stats.rlp(encoding);
  db_->insert(write_batch, DB::Columns::block_rewards_stats, period, encoding.out());
}

uint32_t Stats::getCurrentDistributionFrequency(uint64_t current_block) const {
  auto distribution_frequencies = kHardforksConfig.rewards_distribution_frequency;
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

BlockStats Stats::getBlockStats(const PeriodData& blk, const std::vector<gas_t>& trxs_fees) {
  uint64_t dpos_vote_count = kCommitteeSize;

  // Block zero
  if (!blk.previous_block_cert_votes.empty()) [[likely]] {
    dpos_vote_count = dpos_eligible_total_vote_count_(blk.previous_block_cert_votes[0]->getPeriod() - 1);
  }
  if (blk.pbft_blk->getPeriod() < kHardforksConfig.magnolia_hf.block_num) {
    return BlockStats{blk, {}, dpos_vote_count, kCommitteeSize};
  }

  const auto aspen_hf_part_one = kHardforksConfig.isAspenHardforkPartOne(blk.pbft_blk->getPeriod());
  return BlockStats{blk, trxs_fees, dpos_vote_count, kCommitteeSize, aspen_hf_part_one};
}

std::vector<BlockStats> Stats::processStats(const PeriodData& current_blk, const std::vector<gas_t>& trxs_gas_used,
                                            DbStorage::Batch& write_batch) {
  const auto current_period = current_blk.pbft_blk->getPeriod();
  const auto frequency = getCurrentDistributionFrequency(current_period);
  auto block_stats = getBlockStats(current_blk, trxs_gas_used);

  // Distribute rewards every block
  if (frequency == 1) {
    return {block_stats};
  }

  blocks_stats_.emplace(current_period, std::move(block_stats));
  // Blocks between distribution. Process and save for future processing
  if (current_period % frequency != 0) {
    // Save to db, so in case of restart data could be just loaded for the period
    saveBlockStats(current_period, blocks_stats_[current_period], write_batch);
    return {};
  }

  // Transform ordered (or unordered) blocks stats into vector
  auto transformStatsToVector = [](auto&& blocks_stats) {
    std::vector<BlockStats> stats_vec;
    stats_vec.reserve(blocks_stats.size());

    std::transform(std::make_move_iterator(blocks_stats.begin()), std::make_move_iterator(blocks_stats.end()),
                   std::back_inserter(stats_vec), [](auto&& t) { return std::move(t.second); });
    return stats_vec;
  };

  std::vector<BlockStats> res;

  // Blocks stats were not sorted by period before aspen hardfork part one
  if (current_period < kHardforksConfig.aspen_hf.block_num_part_one) {
    res = transformStatsToVector(std::move(blocks_stats_));
  } else {
    // Blocks stats are sorted by period after aspen hardfork part one
    std::map<PbftPeriod, BlockStats> ordered_blocks_stats;
    std::transform(std::make_move_iterator(blocks_stats_.begin()), std::make_move_iterator(blocks_stats_.end()),
                   std::inserter(ordered_blocks_stats, ordered_blocks_stats.end()),
                   [](auto&& t) { return std::move(t); });

    res = transformStatsToVector(std::move(ordered_blocks_stats));
  }
  return res;
}

}  // namespace taraxa::rewards