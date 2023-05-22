#include "config/hardfork.hpp"
#include "rewards/block_stats.hpp"
#include "storage/storage.hpp"

namespace taraxa::rewards {
/**
 * @brief class that is managing rewards stats processing and hardforks(intervals changes)
 * So intermediate blocks stats are stored in the vector in data(to restore on the node restart)
 * and full list of interval stats is returned in the end of interval
 */
class Stats {
 public:
  Stats(uint32_t committee_size, const Hardforks& hardforks, std::shared_ptr<DB> db,
        std::function<uint64_t(EthBlockNumber)>&& dpos_eligible_total_vote_count);

  /**
   * @brief processing passed block and returns stats that should be processed at this block
   * @param current_blk block to process
   * @return vector<BlockStats> that should be processed at current block
   */
  std::vector<BlockStats> processStats(const PeriodData& current_blk, const std::vector<gas_t>& trxs_gas_used);

 protected:
  /**
   * @brief load current interval stats from database
   */
  void loadFromDb();
  /**
   * @brief returns rewards distribution frequency for specified period
   */
  uint32_t getCurrentDistributionFrequency(uint64_t current_period) const;
  /*
   * @brief gets all needed data and makes(processes) BlocksStats
   * @param current_blk block to process
   * @return block statistics needed for rewards distribution
   */
  BlockStats getBlockStats(const PeriodData& current_blk, std::vector<gas_t> trxs_fees);
  /**
   * @brief saves stats to database to not lose this data in case of node restart
   */
  void saveBlockStats(uint64_t number, const BlockStats& stats);
  /**
   * @brief called on start of new rewards interval. clears blocks_stats_ collection
   * and removes all data saved in db column
   */
  void clear();

  const uint32_t kCommitteeSize;
  const Hardforks kHardforks;
  std::shared_ptr<DB> db_;
  const std::function<uint64_t(EthBlockNumber)> dpos_eligible_total_vote_count_;
  std::vector<BlockStats> blocks_stats_;
};
}  // namespace taraxa::rewards