#ifndef TARAXA_NODE_UTIL_OWNING_SHARED_PTR_HPP_
#define TARAXA_NODE_UTIL_OWNING_SHARED_PTR_HPP_

#include <memory>
#include <type_traits>

namespace taraxa::util::owning_shared_ptr {
using std::default_delete;
using std::forward;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

template <typename T, typename Deleter = default_delete<T>>
struct owning_shared_ptr : public shared_ptr<T> {
  using shared_t = shared_ptr<T>;
  using weak_t = weak_ptr<T>;
  using deleter_t = Deleter;
  using unique_t = unique_ptr<T, deleter_t>;

 private:
  deleter_t deleter_;

 public:
  owning_shared_ptr(owning_shared_ptr const&) = delete;
  owning_shared_ptr& operator=(owning_shared_ptr const&) = delete;

  owning_shared_ptr(owning_shared_ptr&&) = default;
  owning_shared_ptr& operator=(owning_shared_ptr&&) = default;

  owning_shared_ptr() = default;

  owning_shared_ptr(T* ptr, deleter_t const& deleter = deleter_t())
      : shared_t(ptr, [](...) {}), deleter_(deleter) {}

  owning_shared_ptr(unique_t&& ptr)
      : owning_shared_ptr(ptr.release(), ptr.get_deleter()) {}

  ~owning_shared_ptr() {
    auto ptr = this->get();
    if (!ptr) {
      return;
    }
    weak_t w_ptr = *this;
    this->reset();
    while (!w_ptr.expired()) {
    }
    deleter_(ptr);
  }

  template <typename... Args>
  static owning_shared_ptr make(Args&&... args) {
    return make_unique<T>(forward<Args>(args)...);
  }
};

}  // namespace taraxa::util::owning_shared_ptr

#endif  // TARAXA_NODE_UTIL_OWNING_SHARED_PTR_HPP_
