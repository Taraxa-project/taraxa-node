#include "common/thread_pool.hpp"

#include <iostream>

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

void ThreadPool::post(uint64_t do_in_ms, asio_callback action) {
  // std::cout << "Into ThreadPool::post1" << std::endl;
  ++num_pending_tasks_;
  // std::cout << "&&&&&&&&&&&&&& post1 do in ms " << do_in_ms << std::endl;
  if (!do_in_ms) {
    boost::asio::post(ioc_, [this, action = std::move(action)] {
      action({});
      --num_pending_tasks_;
      // std::cout << "************************ num pending task --" << std::endl;
    });
    return;
  }
  auto timer = std::make_shared<boost::asio::deadline_timer>(ioc_);
  timer->expires_from_now(boost::posix_time::milliseconds(do_in_ms));
  timer->async_wait([this, action = std::move(action), timer](auto const &err_code) {
    action(err_code);
    --num_pending_tasks_;
  });
}

void ThreadPool::post(uint64_t do_in_ms, std::function<void()> action) {
  // std::cout << "Into ThreadPool::post2" << std::endl;
  // std::cout << "################ post2 do in ms " << do_in_ms << std::endl;
  post(do_in_ms, [action = std::move(action)](auto const &err) {
    if (!err) {
      action();
      // std::cout << "************************* Finish action " << std::endl;
      return;
    }
    assert(err == boost::asio::error::operation_aborted);
  });
}

void ThreadPool::post_loop(Periodicity const &periodicity, std::function<void()> action) {
  post(periodicity.delay_ms, [this, periodicity, action = move(action)]() mutable {
    action();
    post_loop({periodicity.period_ms}, move(action));
  });
}

ThreadPool::~ThreadPool() { stop(); }

}  // namespace taraxa::util
