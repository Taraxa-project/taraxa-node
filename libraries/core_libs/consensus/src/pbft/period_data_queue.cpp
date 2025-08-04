#include "pbft/period_data_queue.hpp"

#include "dag/dag_block.hpp"
#include "pbft/pbft_chain.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

PeriodDataQueue::PeriodDataQueue(const FullNodeConfig& conf) : kConfig(conf) {}

uint64_t PeriodDataQueue::getPeriod() const {
  std::shared_lock lock(queue_access_);
  return period_;
}

size_t PeriodDataQueue::size() const {
  std::shared_lock lock(queue_access_);
  if (queue_.empty()) {
    return 0;
  }

  size_t size = queue_.size();

  // Each period data requires cert votes from the next block or from the cert_votes if it is the last
  // block, a block is counted only if it has cert votes as well
  if (queue_.back().cert_votes.empty()) {
    size--;
  }

  if (size > 1) {
    auto& second_last_item = queue_[size - 2];
    if (kConfig.genesis.state.hardforks.isOnFragariaHardfork(second_last_item.period_data.pbft_blk->getPeriod()) &&
        second_last_item.cert_votes.empty()) {
      size--;
    }
  }

  return size;
}

bool PeriodDataQueue::empty() const {
  std::shared_lock lock(queue_access_);
  return queue_.empty();
}

void PeriodDataQueue::clear() {
  std::unique_lock lock(queue_access_);
  period_ = 0;
  queue_.clear();
  last_popped_data_.reset();
}

bool PeriodDataQueue::push(QueueData&& queue_data, uint64_t max_pbft_size) {
  const auto period = queue_data.period_data.pbft_blk->getPeriod();
  std::unique_lock lock(queue_access_);

  // It needs to be block after the last block in the queue or max_pbft_size + 1 or max_pbft_size + 2 since it is
  // possible that block max_pbft_size + 1 is removed from the queue but not yet pushed in the chain
  if (period != std::max(period_, max_pbft_size) + 1 && (queue_.empty() && period != max_pbft_size + 2)) {
    return false;
  }

  if (max_pbft_size > period_ && !queue_.empty()) {
    queue_.clear();
  }

  period_ = period;
  queue_.emplace_back(std::move(queue_data));

  return true;
}

std::optional<PeriodDataQueue::QueueData> PeriodDataQueue::pop() {
  std::unique_lock lock(queue_access_);
  auto queue_data = queue_.front();
  last_popped_data_ = {queue_data.period_data.pbft_blk->getBlockHash(), queue_data.period_data.pbft_blk->getPeriod()};

  if (!queue_data.cert_votes.empty()) {
    queue_.pop_front();
    // if queue is empty set period to zero
    if (queue_.empty()) {
      period_ = 0;
    }

    return std::move(queue_data);
  }

  size_t cert_votes_queue_data_idx =
      (kConfig.genesis.state.hardforks.isOnFragariaHardfork(queue_data.period_data.pbft_blk->getPeriod()) ? 2 : 1);
  if (queue_.size() <= cert_votes_queue_data_idx) {
    // This should almost never happen
    return std::nullopt;
  }

  queue_data.cert_votes = queue_[cert_votes_queue_data_idx].period_data.reward_votes_;
  queue_.pop_front();

  return std::move(queue_data);
}

std::shared_ptr<PbftBlock> PeriodDataQueue::lastPbftBlock() const {
  std::shared_lock lock(queue_access_);
  if (!queue_.empty()) {
    return queue_.back().period_data.pbft_blk;
  }

  return nullptr;
}

std::shared_ptr<PbftBlock> PeriodDataQueue::secondLastPbftBlock() const {
  std::shared_lock lock(queue_access_);
  if (queue_.size() > 1) {
    return (queue_.end() - 2)->period_data.pbft_blk;
  }

  return nullptr;
}

std::optional<std::pair<blk_hash_t, PbftPeriod>> PeriodDataQueue::getLastPoppedData() const {
  std::shared_lock lock(queue_access_);
  return last_popped_data_;
}

void PeriodDataQueue::cleanOldData(uint64_t period) {
  std::unique_lock lock(queue_access_);
  while (queue_.size() > 0 && queue_.front().period_data.pbft_blk->getPeriod() < period) {
    queue_.pop_front();
  }
}

}  // namespace taraxa