#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace taraxa::util::signal {
using namespace std;

template <typename T>
class Signal {
  mutex mu_;
  condition_variable cv_;
  T value_;
  bool value_present_ = false;

 public:
  void emit(T const& value) { emit(T(value)); }

  void emit(T&& value) {
    {
      unique_lock l(mu_);
      value_ = move(value);
      value_present_ = true;
    }
    cv_.notify_one();
  }

  T await() {
    T ret;
    unique_lock l(mu_);
    while (!value_present_) {
      cv_.wait(l);
    }
    swap(ret, value_);
    value_present_ = false;
    return ret;
  }
};

}  // namespace taraxa::util::signal

namespace taraxa::util {
using signal::Signal;
}