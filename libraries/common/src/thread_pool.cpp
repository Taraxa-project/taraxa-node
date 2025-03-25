#include "common/thread_pool.hpp"

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
  if (!threads_.empty()) {
    return;
  }
  for (uint i = 0; i < threads_.capacity(); ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }
}

bool ThreadPool::is_running() const { return !threads_.empty(); }

void ThreadPool::stop() {
  ioc_.stop();
  for (auto &th : threads_) {
    if (th.joinable()) {
      th.join();
    }
  }
  threads_.clear();
  ioc_.restart();  // for potential restart
}

std::future<void> ThreadPool::post(uint64_t do_in_ms, asio_callback action) {
  auto task = std::make_shared<std::packaged_task<void(boost::system::error_code)>>(std::move(action));
  std::future<void> future = task->get_future();
  ++num_pending_tasks_;
  if (!do_in_ms) {
    boost::asio::post(ioc_, [this, task] {
      (*task)({});
      --num_pending_tasks_;
    });
    return future;
  }
  auto timer = std::make_shared<boost::asio::deadline_timer>(ioc_);
  timer->expires_from_now(boost::posix_time::milliseconds(do_in_ms));
  timer->async_wait([this, task, timer](boost::system::error_code const &err_code) {
    (*task)(err_code);
    --num_pending_tasks_;
  });
  return future;
}

std::future<void> ThreadPool::post(uint64_t do_in_ms, std::function<void()> action) {
  return post(do_in_ms, [action = std::move(action)](auto const &err) {
    if (!err) {
      action();
      return;
    }
    assert(err == boost::asio::error::operation_aborted);
  });
}

void ThreadPool::post_loop(Periodicity const &periodicity, std::function<void()> action) {
  post(periodicity.delay_ms, [this, periodicity, action = std::move(action)]() mutable {
    action();
    post_loop({periodicity.period_ms}, std::move(action));
  });
}

ThreadPool::~ThreadPool() { stop(); }

}  // namespace taraxa::util
