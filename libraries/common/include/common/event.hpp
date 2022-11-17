#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>

#include "common/functional.hpp"

namespace taraxa::util::event {

template <typename Payload>
struct EventEmitter;

template <typename Payload>
struct EventSubscriber {
  using Handler = std::function<void(Payload const &)>;

 protected:
  class State {
    friend struct EventSubscriber;
    friend struct EventEmitter<Payload>;

    uint64_t next_subscription_id_ = 0;
    std::map<uint64_t, Handler> subs_;
    std::shared_mutex mu_;
  } mutable state_;

  EventSubscriber() = default;

 public:
  auto subscribe(Handler &&handler, task_executor_t &&execution_context = current_thread_executor()) const {
    std::unique_lock l(state_.mu_);
    auto subscription_id = ++state_.next_subscription_id_;
    state_.subs_[subscription_id] = [exec = std::move(execution_context), h = std::move(handler)](auto const &payload) {
      exec([&h, payload] { h(payload); });
    };
    return subscription_id;
  }

  auto unsubscribe(uint64_t subscription_id) const {
    std::unique_lock l(state_.mu_);
    return state_.subs_.erase(subscription_id);
  }
};

template <typename Payload>
struct EventEmitter : virtual EventSubscriber<Payload> {
  using Subscriber = EventSubscriber<Payload>;

  void emit(Payload const &payload) const {
    std::shared_lock l(Subscriber::state_.mu_);
    for (auto const &[_, subscription] : Subscriber::state_.subs_) {
      subscription(payload);
    }
  }
};

template <class Owner, typename Payload>
class Event : virtual EventEmitter<Payload>, public virtual EventSubscriber<Payload> {
  friend Owner;
};

}  // namespace taraxa::util::event

namespace taraxa::util {
using event::Event;
using event::EventEmitter;
using event::EventSubscriber;
}  // namespace taraxa::util
