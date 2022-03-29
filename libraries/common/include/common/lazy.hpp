#pragma once

#include <cassert>
#include <functional>
#include <mutex>
#include <type_traits>

namespace taraxa::util::lazy {
using std::enable_if_t;
using std::function;
using std::is_invocable_v;
using std::mutex;
using std::remove_reference;
using std::size_t;
using std::unique_lock;

// TODO [1657]: clean up this class - remove ned/delete, ideally remove whole Lazy class !!!
template <typename Provider>
class Lazy {
  Provider* provider_ = nullptr;

 public:
  using val_t = typename remove_reference<decltype((*provider_)())>::type;

 private:
  val_t* val_ptr_ = nullptr;
  mutex mu_;
  bool initializing_ = false;

 public:
  Lazy(Provider const& provider) : provider_(new Provider(provider)) {}
  Lazy(Provider&& provider) : provider_(new Provider(provider)) {}
  ~Lazy() {
    if (provider_) {
      delete provider_;
    }
    if (val_ptr_) {
      delete val_ptr_;
    }
  }

  val_t* get() {
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
    val_ptr_ = new val_t((*provider_)());
    delete provider_;
    provider_ = nullptr;
    initializing_ = false;
    return val_ptr_;
  }

  val_t const* get() const { return const_cast<val_t const*>(const_cast<Lazy*>(this)->get()); }

  val_t* operator->() { return get(); }
  val_t const* operator->() const { return get(); }

  val_t& operator*() { return *operator->(); }
  val_t const& operator*() const { return *operator->(); }

  // clang-format off
  operator val_t&() { return operator*(); }
  operator val_t const &() const { return operator*(); }
  // clang-format on

  template <typename T>
  auto& operator[](T t) {
    return operator*()[t];
  }

  template <typename T>
  auto const& operator[](T t) const {
    return operator*()[t];
  }

  template <typename T>
  auto& operator=(T t) {
    return operator*() = t;
  }
};

template <typename T>
using LazyVal = Lazy<function<T()>>;

}  // namespace taraxa::util::lazy
