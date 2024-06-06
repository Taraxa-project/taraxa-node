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

bool PeriodDataQueue::push(PeriodData &&period_data, const dev::p2p::NodeID &node_id, uint64_t max_pbft_size,
                           std::vector<std::shared_ptr<PbftVote>> &&cert_votes) {
  const auto period = period_data.pbft_blk->getPeriod();
  std::unique_lock lock(queue_access_);

  // It needs to be block after the last block in the queue or max_pbft_size + 1 or max_pbft_size + 2 since it is
  // possible that block max_pbft_size + 1 is removed from the queue but not yet pushed in the chain
  if (period != std::max(period_, max_pbft_size) + 1 && (queue_.empty() && period != max_pbft_size + 2)) {
    return false;
  }
  if (max_pbft_size > period_ && !queue_.empty()) queue_.clear();
  period_ = period;
  queue_.emplace_back(std::move(period_data), node_id);
  last_block_cert_votes_ = std::move(cert_votes);
  return true;
}

std::tuple<PeriodData, std::vector<std::shared_ptr<PbftVote>>, dev::p2p::NodeID> PeriodDataQueue::pop() {
  std::unique_lock lock(queue_access_);
  auto block = std::move(queue_.front());
  queue_.pop_front();
  if (queue_.size() > 0)
    return {block.first, queue_.front().first.previous_block_cert_votes, block.second};
  else {
    // if queue is empty set period to zero and move last_block_cert_votes_
    period_ = 0;
    auto cert_votes = std::move(last_block_cert_votes_);
    last_block_cert_votes_.clear();
    return {block.first, std::move(cert_votes), block.second};
  }
}

std::shared_ptr<PbftBlock> PeriodDataQueue::lastPbftBlock() const {
  std::shared_lock lock(queue_access_);
  if (queue_.size() > 0) {
    return queue_.back().first.pbft_blk;
  }
  return nullptr;
}

void PeriodDataQueue::cleanOldData(uint64_t period) {
  std::unique_lock lock(queue_access_);
  while (queue_.size() > 0 && queue_.front().first.pbft_blk->getPeriod() < period) {
    queue_.pop_front();
  }
}

}  // namespace taraxa