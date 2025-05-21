#pragma once

#include <boost/asio.hpp>

namespace taraxa::util {

class ThreadPool : std::enable_shared_from_this<ThreadPool> {
  using asio_callback = std::function<void(boost::system::error_code const &)>;

  boost::asio::io_context ioc_;
  boost::asio::executor_work_guard<decltype(ioc_)::executor_type> ioc_work_;
  std::vector<std::thread> threads_;

  std::atomic<uint64_t> num_pending_tasks_ = 0;

 public:
  explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency(), bool _start = true);
  ~ThreadPool();

  ThreadPool(ThreadPool const &) = delete;
  ThreadPool &operator=(ThreadPool const &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  auto capacity() const { return threads_.capacity(); }
  uint64_t num_pending_tasks() const { return num_pending_tasks_; }

  // TODO eliminate
  auto &unsafe_get_io_context() { return ioc_; }

  void start();
  bool is_running() const;
  void stop();

  std::future<void> post(uint64_t do_in_ms, asio_callback action);
  std::future<void> post(uint64_t do_in_ms, std::function<void()> action);

  template <typename Action>
  std::future<void> post(Action &&action) {
    return post(0, std::forward<Action>(action));
  }

  struct Periodicity {
    uint64_t period_ms = 0, delay_ms = period_ms;
  };
  void post_loop(Periodicity const &periodicity, std::function<void()> action);
};
}  // namespace taraxa::util