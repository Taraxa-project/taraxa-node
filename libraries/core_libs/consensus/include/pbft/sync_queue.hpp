#pragma once

#include <libp2p/Host.h>

#include <deque>

#include "pbft/sync_block.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class SyncBlock;

/**
 * @brief SyncBlockQueue class is a syncing queue, the queue stores blocks synced from peers
 */
class SyncBlockQueue {
 public:
  SyncBlockQueue() = default;

  /**
   * @brief Push a synced block in queue
   * @param block block synced from peer
   * @param node_id peer node ID
   * @param max_pbft_size maximum PBFT chain size
   * @return true if pushed
   */
  bool push(SyncBlock &&block, dev::p2p::NodeID const &node_id, uint64_t max_pbft_size);

  /**
   * @brief Pop the first block from syncing queue
   * @return the first block and peer node ID
   */
  std::pair<SyncBlock, dev::p2p::NodeID> pop();

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

 private:
  std::deque<std::pair<SyncBlock, dev::p2p::NodeID>> queue_;
  // We need this variable as for small amount of time block is not part of queue but still being processed
  uint64_t period_{0};
  mutable std::shared_mutex queue_access_;
};

/** @}*/

}  // namespace taraxa