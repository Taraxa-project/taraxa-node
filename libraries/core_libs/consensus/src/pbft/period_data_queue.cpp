#include "pbft/period_data_queue.hpp"

#include "dag/dag_block.hpp"
#include "pbft/pbft_chain.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

uint64_t PeriodDataQueue::getPeriod() const {
  std::shared_lock lock(queue_access_);
  return period_;
}

size_t PeriodDataQueue::size() const {
  std::shared_lock lock(queue_access_);
  // Each period data requires cert votes from the next block or from the last_block_cert_votes_ if it is the last
  // block, a block is counted only if it has cert votes as well
  if (last_block_cert_votes_.size() || queue_.size() == 0)
    return queue_.size();
  else
    return queue_.size() - 1;
}

bool PeriodDataQueue::empty() const {
  std::shared_lock lock(queue_access_);
  return queue_.empty();
}

void PeriodDataQueue::clear() {
  std::unique_lock lock(queue_access_);
  period_ = 0;
  queue_.clear();
  last_block_cert_votes_.clear();
}

bool PeriodDataQueue::push(PeriodData &&period_data, dev::p2p::NodeID const &node_id, uint64_t max_pbft_size,
                           std::vector<std::shared_ptr<Vote>> &&cert_votes) {
  const auto period = period_data.pbft_blk->getPeriod();
  std::unique_lock lock(queue_access_);

  if (period != std::max(period_, max_pbft_size) + 1) {
    return false;
  }
  if (max_pbft_size > period_ && !queue_.empty()) queue_.clear();
  period_ = period;
  queue_.emplace_back(std::move(period_data), node_id);
  last_block_cert_votes_ = std::move(cert_votes);
  return true;
}

std::tuple<PeriodData, std::vector<std::shared_ptr<Vote>>, dev::p2p::NodeID> PeriodDataQueue::pop() {
  std::unique_lock lock(queue_access_);
  auto block = std::move(queue_.front());
  queue_.pop_front();
  if (queue_.size() > 0)
    return {block.first, queue_.front().first.previous_block_cert_votes, block.second};
  else
    return {block.first, last_block_cert_votes_, block.second};
}

std::shared_ptr<PbftBlock> PeriodDataQueue::lastPbftBlock() const {
  std::shared_lock lock(queue_access_);
  if (queue_.size() > 0) {
    return queue_.back().first.pbft_blk;
  }
  return nullptr;
}

}  // namespace taraxa