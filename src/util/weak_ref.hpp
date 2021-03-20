#pragma once

#include <functional>
#include <shared_mutex>

namespace taraxa::util {

template <typename T>
class WeakRef {
  mutable std::shared_mutex mu_;
  std::weak_ptr<T> wp_;

 public:
  void reset(std::weak_ptr<T>&& wp = {}) {
    std::unique_lock l(mu_);
    wp_ = std::move(wp);
  }

  template <typename Visitor>
  auto visit(Visitor const& visitor, decltype(visitor(wp_.lock().get())) const& default_return = {}) const {
    std::shared_ptr<T> p;
    {
      std::shared_lock l(mu_);
      p = wp_.lock();
    }
    return p ? visitor(p.get()) : default_return;
  }

  auto visit(std::function<void(T*)> const& visitor) const {
    std::shared_ptr<T> p;
    {
      std::shared_lock l(mu_);
      p = wp_.lock();
    }
    if (p) {
      visitor(p.get());
    }
  }
};

}  // namespace taraxa::util