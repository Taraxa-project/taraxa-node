#pragma once

#include <boost/asio.hpp>
#include <shared_mutex>

#include "functional.hpp"

namespace taraxa::util {

class ThreadPool : std::enable_shared_from_this<ThreadPool> {
  using asio_callback = std::function<void(boost::system::error_code const &)>;

  boost::asio::io_context ioc_;
  boost::asio::executor_work_guard<decltype(ioc_)::executor_type> ioc_work_;
  std::vector<std::thread> threads_;
  mutable std::shared_mutex threads_mu_;

  std::atomic<uint64_t> debug_num_pending_tasks_ = 0;

 public:
  explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency(), bool _start = true);
  ~ThreadPool();

  ThreadPool(ThreadPool const &) = delete;
  ThreadPool &operator=(ThreadPool const &) = delete;

  auto capacity() const { return threads_.capacity(); }
  uint64_t num_pending_tasks() const { return debug_num_pending_tasks_; }

  // TODO eliminate
  auto &unsafe_get_io_context() { return ioc_; }

  void start();
  bool is_running() const;
  void stop();

  void post(uint64_t do_in_ms, asio_callback action);
  void post(uint64_t err, std::function<void()> action);

  template <typename Action>
  auto post(Action &&action) {
    return post(0, std::forward<Action>(action));
  }

  struct Periodicity {
    uint64_t period_ms = 0, delay_ms = period_ms;
  };
  void post_loop(Periodicity const &periodicity, std::function<void()> action);

  operator task_executor_t() {
    return [this](auto &&task) { post(std::forward<task_t>(task)); };
  }

  task_executor_t strand() {
    return [s = boost::asio::make_strand(ioc_)](auto &&task) { boost::asio::post(s, std::forward<task_t>(task)); };
  }
};

}  // namespace taraxa::util