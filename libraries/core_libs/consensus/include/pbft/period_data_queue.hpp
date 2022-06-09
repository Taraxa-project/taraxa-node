#pragma once

#include <libp2p/Host.h>

#include <deque>

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
  PeriodDataQueue() = default;

  /**
   * @brief Push a synced block in queue
   * @param period_data period_data synced from peer
   * @param node_id peer node ID
   * @param max_pbft_size maximum PBFT chain size
   * @param cert_votes cert votes
   * @return true if pushed
   */
  bool push(PeriodData &&period_data, dev::p2p::NodeID const &node_id, uint64_t max_pbft_size,
            std::vector<std::shared_ptr<Vote>> &&cert_votes);

  /**
   * @brief Pop the first block from syncing queue
   * @return the first block, votes for the block if they are available and peer node ID
   */
  std::tuple<PeriodData, std::vector<std::shared_ptr<Vote>>, dev::p2p::NodeID> pop();

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
   * @brief Get period number of the last synced block in queue
   * @return period number of the last synced block in queue. If syncing queue is empty, return 0
   */
  uint64_t getPeriod() const;

  /**
   * @brief Get last pbft block from queue
   * @return last block or nullptr if queue empty
   */
  std::shared_ptr<PbftBlock> lastPbftBlock();

 private:
  std::deque<std::pair<PeriodData, dev::p2p::NodeID>> queue_;
  // We need this variable as for small amount of time block is not part of queue but still being processed
  uint64_t period_{0};
  mutable std::shared_mutex queue_access_;
  // Once fully synced, this will keep the cert votes for the last block in the chain
  std::vector<std::shared_ptr<Vote>> last_block_cert_votes_;
};

/** @}*/

}  // namespace taraxa