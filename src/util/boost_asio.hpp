#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <chrono>

namespace taraxa::util {

using asio_callback = std::function<void(boost::system::error_code const&)>;

template <typename Executor>
void post(Executor& executor, uint64_t do_in_ms, asio_callback action) {
  if (!do_in_ms) {
    ba::post(executor, [action = std::move(action)] { action({}); });
    return;
  }
  auto timer = std::make_shared<ba::deadline_timer>(executor);
  timer->expires_from_now(boost::posix_time::milliseconds(do_in_ms));
  timer->async_wait([action = std::move(action), timer](auto const& err_code) { action(err_code); });
}

template <typename Executor>
void post(Executor& executor, uint64_t do_in_ms, std::function<void()> action) {
  post(executor, do_in_ms, [action = std::move(action)](auto const& err_code) {
    if (!err_code) {
      action();
    }
  });
}

struct Periodicity {
  uint64_t period_ms = 0, delay_ms = period_ms;
};
template <typename Executor>
void post_periodic(Executor& executor, Periodicity const& periodicity, std::function<void()> action) {
  // TODO avoid copying `action` every iteration
  post(executor, periodicity.delay_ms, [=, &executor] {
    action();
    post_periodic(executor, {periodicity.period_ms}, action);
  });
}

}  // namespace taraxa::util