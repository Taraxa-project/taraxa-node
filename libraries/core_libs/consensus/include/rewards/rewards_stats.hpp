#include "config/hardfork.hpp"
#include "rewards/block_stats.hpp"

namespace taraxa::rewards {
class Stats {
 public:
  Stats(uint32_t committee_size, std::function<uint64_t(EthBlockNumber)>&& dpos_eligible_total_vote_count);

  std::vector<BlockStats> getStats(const PeriodData& current_blk);

 private:
  BlockStats getBlockStats(const PeriodData& current_blk);

  const uint32_t kCommitteeSize;
  const std::function<uint64_t(EthBlockNumber)> dpos_eligible_total_vote_count_;
};
}  // namespace taraxa::rewards