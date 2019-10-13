#ifndef TARAXA_NODE_UTIL_PROCESS_STATE_HPP_
#define TARAXA_NODE_UTIL_PROCESS_STATE_HPP_

#include <atomic>
#include <shared_mutex>

namespace taraxa::util::process_state {
using std::atomic;
using std::shared_lock;
using std::shared_mutex;
using std::unique_lock;

struct ProcessState {
  enum Value { runnable, running, stopped };

 private:
  Value value_;
  shared_mutex m_;

 public:
  class Assumption {
    unique_lock<shared_mutex> lock_;
    ProcessState

   public:
    Assumption(ProcessState const& process_state, Value const& value)
        : lock_(process_state.m_) {
      if (value != process_state.value_) {
        lock_.unlock();
      }
    }

    operator bool() { return bool(lock_); }

    void transitionTo(Value const& to) {
      assert(this);
//      assert((to == runnable ? stopped : to - 1) == to);
    }
  };
};

}  // namespace taraxa::util::process_state

#endif  // TARAXA_NODE_UTIL_PROCESS_STATE_HPP_
