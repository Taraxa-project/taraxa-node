#ifndef TARAXA_NODE_UTIL_LAZY_HPP_
#define TARAXA_NODE_UTIL_LAZY_HPP_

#include <functional>
#include <mutex>
#include <type_traits>

namespace taraxa::util::lazy {
using std::mutex;
using std::remove_reference;
using std::size_t;
using std::unique_lock;

template <typename Provider>
class Lazy {
  Provider* provider_ = nullptr;

 public:
  using value_type = typename remove_reference<decltype((*provider_)())>::type;

 private:
  value_type* val_ptr_ = nullptr;
  mutex mu_;
  bool initializing_ = false;

 public:
  Lazy(Provider const& provider) : provider_(new Provider(provider)){};
  Lazy(Provider&& provider) : provider_(new Provider(provider)){};
  ~Lazy() {
    if (provider_) {
      delete provider_;
    }
    if (val_ptr_) {
      delete val_ptr_;
    }
  }

  value_type* get() {
    if (val_ptr_) {
      return val_ptr_;
    }
    unique_lock l(mu_);
    if (val_ptr_) {
      return val_ptr_;
    }
    // this will persist any error that happened during initialization
    assert(!initializing_);
    initializing_ = true;
    val_ptr_ = new value_type((*provider_)());
    delete provider_;
    provider_ = nullptr;
    initializing_ = false;
    return val_ptr_;
  }

  value_type const* get() const {
    return const_cast<value_type const*>(const_cast<Lazy*>(this)->get());
  }

  value_type* operator->() { return get(); }
  value_type const* operator->() const { return get(); }

  value_type& operator*() { return *operator->(); }
  value_type const& operator*() const { return *operator->(); }

  operator value_type&() { return operator*(); }
  operator value_type const&() const { operator*(); }

  auto& operator[](size_t i) { return operator*()[i]; }
  auto const& operator[](size_t i) const { return operator*()[i]; }

  template <typename T>
  auto& operator=(T t) {
    return operator*() = t;
  }
};

}  // namespace taraxa::util::lazy

#endif  // TARAXA_NODE_UTIL_LAZY_HPP_
