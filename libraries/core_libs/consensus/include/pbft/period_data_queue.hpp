#pragma once

#include <libp2p/Host.h>

#include <deque>

#include "config/config.hpp"
#include "pbft/period_data.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class PeriodData;

/**
 * @brief PeriodDataQueue class is a syncing queue, the queue stores blocks synced from peers
 */
class PeriodDataQueue {
 public:
  struct QueueData {
    dev::p2p::NodeID node_id;
    PeriodData period_data;
    std::vector<std::shared_ptr<PbftVote>> cert_votes;
  };

 public:
  PeriodDataQueue(const FullNodeConfig& conf);

  /**
   * @brief Push a synced block in queue
   * @param queue_data period_data synced from peer
   * @param max_pbft_size maximum PBFT chain size
   * @return true if pushed
   */
  bool push(QueueData&& queue_data, uint64_t max_pbft_size);

  /**
   * @brief Pop the first block from syncing queue
   *
   * @return the first block, votes for the block if they are available and peer node ID
   */
  std::optional<QueueData> pop();

  /**
   * @brief Clear the syncing queue and reset period to 0
   */
  void clear();

  /**
   * @brief Get the syncing queue size
   * @return syncing queue size
   */
  size_t size() const;

  /**
   * @brief Return true if the queue is empty
   * @return
   */
  bool empty() const;

  /**
   * @brief Get period number of the last synced block in queue
   * @return period number of the last synced block in queue. If syncing queue is empty, return 0
   */
  uint64_t getPeriod() const;

  /**
   * @brief Get last pbft block from queue
   * @return last block or nullptr if queue empty
   */
  std::shared_ptr<PbftBlock> lastPbftBlock() const;

  /**
   * @brief Get second last pbft block from queue
   * @return second last block or nullptr if queue empty
   */
  std::shared_ptr<PbftBlock> secondLastPbftBlock() const;

  /**
   * @return last popped data
   */
  std::optional<std::pair<blk_hash_t, PbftPeriod>> getLastPoppedData() const;

  /**
   * @brief Clean any old blocks below current period
   * @param period current period
   */
  void cleanOldData(uint64_t period);

 private:
  const FullNodeConfig kConfig;

  std::deque<QueueData> queue_;

  // We need this variable as for small amount of time block is not part of queue but still being processed
  uint64_t period_{0};

  mutable std::shared_mutex queue_access_;

  // Last popped data from queue (probably being processed)
  std::optional<std::pair<blk_hash_t, PbftPeriod>> last_popped_data_;
};

/** @}*/

}  // namespace taraxa