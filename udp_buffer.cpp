/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-17 15:17:50
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 15:18:41
 */

#include "udp_buffer.hpp"

namespace taraxa {

UdpBuffer::UdpBuffer(size_t count, size_t sz)
    : free_qu_(count),
      job_qu_(count),
      mem_pool_(count * sz),
      entries_(count),
      stopped_(false),
      verbose_(false) {
  assert(count > 0);
  auto mems_(mem_pool_.data());
  auto entry_data(entries_.data());
  for (size_t i = 0; i < count; ++i, ++entry_data) {
    *entry_data = {mems_ + i * sz, 0, end_point_udp_t()};
    free_qu_.push_back(entry_data);
  }
}

UdpData *UdpBuffer::allocate() {
  if (verbose_) print("[lock] UdpBuffer allocate ...");
  std::unique_lock<std::mutex> lock(mutex_);

  while (!stopped_ && free_qu_.empty()) {
    if (verbose_) print("[wait] UdpBuffer allocate ...");
    condition_free_qu_.wait(lock);
    if (verbose_) print("[wake up] UdpBuffer allocate ...");
  }

  UdpData *data = nullptr;
  if (!stopped_) {
    if (!free_qu_.empty()) {
      data = free_qu_.front();
      free_qu_.pop_front();
    }
  }
  return data;
}

void UdpBuffer::release(UdpData *data) {
  assert(data != nullptr);
  if (verbose_) print("[lock] UdpBuffer release ...");
  std::unique_lock<std::mutex> lock(mutex_);
  free_qu_.push_back(data);
  condition_free_qu_.notify_one();
  if (verbose_) print("[notify] UdpBuffer release ...");
}

void UdpBuffer::enqueue(UdpData *data) {
  assert(data != nullptr);
  if (verbose_) print("[lock] UdpBuffer enqueue ...");
  std::unique_lock<std::mutex> lock(mutex_);
  job_qu_.push_back(data);
  condition_job_qu_.notify_one();
  if (verbose_) print("[notify] UdpBuffer enqueue ...");
}

UdpData *UdpBuffer::dequeue() {
  if (verbose_) print("[lock] UdpBuffer dequeue ...");
  std::unique_lock<std::mutex> lock(mutex_);
  while (!stopped_ && job_qu_.empty()) {
    if (verbose_) print("[wait] UdpBuffer dequeue ...");
    condition_job_qu_.wait(lock);
    if (verbose_) print("[wake up] UdpBuffer dequeue ...");
  }
  UdpData *data = nullptr;
  if (!job_qu_.empty()) {
    data = job_qu_.front();
    job_qu_.pop_front();
  }
  return data;
}
void UdpBuffer::stop() {
  std::unique_lock<std::mutex> lock(mutex_);
  stopped_ = true;
  condition_job_qu_.notify_all();
  condition_free_qu_.notify_all();
}
bool UdpBuffer::isStopped() {
  std::unique_lock<std::mutex> lock(mutex_);
  return stopped_;
}

void UdpBuffer::print(std::string const &str) {
  std::unique_lock<std::mutex> lock(mutex_for_print_);
  std::cout << str << std::endl;
}

void UdpBuffer::setVerbose(bool verbose) { verbose_ = verbose; }

}  // namespace taraxa
