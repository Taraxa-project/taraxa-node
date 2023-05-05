#include "rewards/rewards_stats.hpp"

#include "storage/storage.hpp"

namespace taraxa::rewards {
Stats::Stats(uint32_t committee_size, std::function<uint64_t(EthBlockNumber)>&& dpos_eligible_total_vote_count)
    : kCommitteeSize(committee_size), dpos_eligible_total_vote_count_(dpos_eligible_total_vote_count) {}

// std::vector<RewardsStats> processBlockHardfork(const PeriodData& current_blk, uint32_t interval) {
//   const auto current = current_blk.pbft_blk->getPeriod();
//   // skip for intermediate blocks
//   if (current % interval != 0) {
//     return {};
//   }

//   std::vector<RewardsStats> rewards_stats;
//   rewards_stats.reserve(interval);
//   // add rewards stats for (last_distribution_block, current_block)
//   for (auto p = current - interval + 1; p < current; ++p) {
//     auto blk = PeriodData(db_->getPeriodDataRaw(p));
//     rewards_stats.emplace_back(get_block_rewards_stats(blk));
//   }
//   // add current block rewards stats
//   rewards_stats.emplace_back(get_block_rewards_stats(current_blk));

//   return rewards_stats;
// }

BlockStats Stats::getBlockStats(const PeriodData& blk) {
  uint64_t dpos_vote_count = kCommitteeSize;

  // Block zero
  if (!blk.previous_block_cert_votes.empty()) [[likely]] {
    dpos_vote_count = dpos_eligible_total_vote_count_(blk.previous_block_cert_votes[0]->getPeriod() - 1);
  }

  return BlockStats{blk, dpos_vote_count, kCommitteeSize};
}

std::vector<BlockStats> Stats::getStats(const PeriodData& current_blk) { return {getBlockStats(current_blk)}; }
}  // namespace taraxa::rewards