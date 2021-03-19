#include "thread_pool.hpp"

namespace taraxa::util {

ThreadPool::ThreadPool(size_t num_threads, bool _start)
    : ioc_(num_threads), ioc_work_(boost::asio::make_work_guard(ioc_)) {
  assert(0 < num_threads);
  threads_.reserve(num_threads);
  if (_start) {
    start();
  }
}

void ThreadPool::start() {
  std::unique_lock l(threads_mu_);
  if (!threads_.empty()) {
    return;
  }
  for (uint i = 0; i < threads_.capacity(); ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }
}

void ThreadPool::stop() {
  std::unique_lock l(threads_mu_);
  ioc_.stop();
  for (auto &th : threads_) {
    th.join();
  }
  threads_.clear();
  ioc_.restart();  // for potential restart
}

ThreadPool::~ThreadPool() { stop(); }

}  // namespace taraxa::util
