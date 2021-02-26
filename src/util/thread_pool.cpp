#include "thread_pool.hpp"

#include "memory.hpp"

namespace taraxa::util {

ThreadPool::ThreadPool(size_t num_threads) : ioc_work_(boost::asio::make_work_guard(ioc_)) {
  threads_.reserve(num_threads);
  for (uint i = 0; i < num_threads; ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }
}

std::shared_ptr<ThreadPool> ThreadPool::make(size_t num_threads) { return s_ptr(new ThreadPool(num_threads)); }

ThreadPool::~ThreadPool() {
  ioc_.stop();
  for (auto &th : threads_) {
    th.join();
  }
}

}  // namespace taraxa::util
