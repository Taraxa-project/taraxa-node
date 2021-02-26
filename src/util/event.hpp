#pragma once

#include <functional>
#include <map>
#include <shared_mutex>

#include "functional.hpp"

namespace taraxa::util::event {
using namespace std;

template <typename Payload>
struct EventEmitter;

template <typename Payload>
struct EventSubscriber {
 protected:
  class State {
    friend class EventSubscriber;
    friend class EventEmitter<Payload>;

    uint64_t next_subscription_id_ = 0;
    map<uint64_t, function<void(shared_ptr<Payload> const &)>> subs_;
    shared_mutex mu_;
  } mutable state_;

  EventSubscriber() = default;

 public:
  auto subscribe(function<void(Payload const &)> &&handler,
                 task_executor_t &&execution_context = current_thread_executor()) const {
    unique_lock l(state_.mu_);
    auto subscription_id = state_.next_subscription_id_++;
    state_.subs_[subscription_id] = [exec = move(execution_context), h = move(handler)](auto const &payload_ptr) {
      exec([&h, payload_ptr] { h(*payload_ptr); });
    };
    return subscription_id;
  }

  auto unsubscribe(uint64_t subscription_id) const {
    unique_lock l(state_.mu_);
    return state_.subs_.erase(subscription_id);
  }
};

template <typename Payload>
struct EventEmitter : virtual EventSubscriber<Payload> {
  using Subscriber = EventSubscriber<Payload>;

  void emit(Payload const &payload) const { emit(make_shared<Payload>(payload)); }
  void emit(Payload &&payload) const { emit(make_shared<Payload>(forward<Payload>(payload))); }

  void emit(shared_ptr<Payload> const &payload_ptr) const {
    shared_lock l(Subscriber::state_.mu_);
    for (auto const &[_, subscription] : Subscriber::state_.subs_) {
      subscription(payload_ptr);
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
