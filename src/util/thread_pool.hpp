#pragma once

#include <boost/asio.hpp>

#include "functional.hpp"

namespace taraxa::util {

class ThreadPool : std::enable_shared_from_this<ThreadPool> {
  boost::asio::io_context ioc_;
  boost::asio::executor_work_guard<decltype(ioc_)::executor_type> ioc_work_;
  std::vector<std::thread> threads_;

 public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  ThreadPool(ThreadPool const &) = delete;
  ThreadPool &operator=(ThreadPool const &) = delete;

  auto &unsafe_get_io_context() { return ioc_; }

  template <typename... T>
  auto post(T &&... args) {
    return boost::asio::post(ioc_, std::forward<T>(args)...);
  }

  operator task_executor_t() {
    return [this](auto &&task) { post(std::forward<task_t>(task)); };
  }
};

}  // namespace taraxa::util