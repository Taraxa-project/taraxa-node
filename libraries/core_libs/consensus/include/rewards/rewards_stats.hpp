#pragma once

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
  Stats(uint32_t committee_size, const HardforksConfig& hardforks, std::shared_ptr<DbStorage> db,
        std::function<uint64_t(EthBlockNumber)>&& dpos_eligible_total_vote_count, EthBlockNumber last_blk_num = 0);

  /**
   * @brief processing passed block and returns stats that should be processed at this block
   * @param current_blk block to process
   * @param blocks_per_year - expected number of blocks generated per year based on pbft block dynamic lambda
   * @return vector<BlockStats> that should be processed at current block
   */
  std::vector<BlockStats> processStats(const PeriodData& current_blk, uint32_t blocks_per_year,
                                       const std::vector<gas_t>& trxs_gas_used, Batch& write_batch);
  /**
   * @brief called on start of new rewards interval. clears blocks_stats_ collection
   * and removes all data saved in db column
   */
  void clear(uint64_t current_period);

 protected:
  /**
   * @brief recover current interval stats from database
   */
  void recoverFromDb(EthBlockNumber last_blk_num);

  /*
   * @brief gets all needed data and makes(processes) BlocksStats
   * @param current_blk block to process
   * @param blocks_per_year - expected number of blocks generated per year based on pbft block dynamic lambda
   * @return block statistics needed for rewards distribution
   */
  BlockStats getBlockStats(const PeriodData& current_blk, uint32_t blocks_per_year,
                           const std::vector<gas_t>& trxs_fees);

  const uint32_t kCommitteeSize;
  const HardforksConfig kHardforksConfig;
  std::shared_ptr<DbStorage> db_;
  const std::function<uint64_t(EthBlockNumber)> dpos_eligible_total_vote_count_;
  std::unordered_map<PbftPeriod, BlockStats> blocks_stats_;
};
}  // namespace taraxa::rewards