#pragma once

#include <libp2p/Host.h>

#include <deque>

#include "pbft/sync_block.hpp"

namespace taraxa {

/** \addtogroup PBFT
 * @{
 */

class SyncBlock;

class SyncBlockQueue {
 public:
  SyncBlockQueue() = default;

  bool push(SyncBlock &&block, dev::p2p::NodeID const &node_id, uint64_t max_pbft_size);
  std::pair<SyncBlock, dev::p2p::NodeID> pop();
  void clear();
  size_t size() const;
  uint64_t getPeriod() const;

 private:
  std::deque<std::pair<SyncBlock, dev::p2p::NodeID>> queue_;
  // We need this variable as for small amount of time block is not part of queue but still being processed
  uint64_t period_{0};
  mutable std::shared_mutex queue_access_;
};

/** @}*/

}  // namespace taraxa