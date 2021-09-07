#include "pbft/sync_queue.hpp"

#include "pbft/chain.hpp"
#include "dag/dag_block.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa {

uint64_t SyncBlockQueue::getPeriod() const {
  std::shared_lock lock(queue_access_);
  return period_;
}

size_t SyncBlockQueue::size() const {
  std::shared_lock lock(queue_access_);
  return queue_.size();
}

void SyncBlockQueue::clear() {
  std::unique_lock lock(queue_access_);
  period_ = 0;
  queue_.clear();
}

bool SyncBlockQueue::push(SyncBlock &&block, dev::p2p::NodeID const &node_id, uint64_t max_pbft_size) {
  const auto period = block.pbft_blk->getPeriod();
  std::unique_lock lock(queue_access_);
  if (period != std::max(period_, max_pbft_size) + 1) {
    return false;
  }
  if (max_pbft_size > period_ && !queue_.empty()) queue_.clear();
  period_ = period;
  queue_.emplace_back(std::move(block), node_id);
  return true;
}

std::pair<SyncBlock, dev::p2p::NodeID> SyncBlockQueue::pop() {
  std::unique_lock lock(queue_access_);
  auto block = std::move(queue_.front());
  queue_.pop_front();
  return block;
}

}  // namespace taraxa