#ifndef TARAXA_NODE_UTIL_PROCESS_CONTAINER_HPP_
#define TARAXA_NODE_UTIL_PROCESS_CONTAINER_HPP_

#include <memory>
#include <type_traits>

namespace taraxa::util::process_container {
using std::default_delete;
using std::forward;
using std::function;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

template <typename Process>
struct process_container : public shared_ptr<Process> {
  using shared_t = shared_ptr<Process>;
  using weak_t = weak_ptr<Process>;
  // not copyable
  process_container(process_container const&) = delete;
  process_container& operator=(process_container const&) = delete;
  // movable
  process_container(process_container&&) = default;
  process_container& operator=(process_container&&) = default;
  // default-constructible
  process_container() = default;
  // constructor is the same as in unique_ptr
  template <typename Deleter = default_delete<Process>, typename... Args>
  process_container(Args&&... args)
      : shared_t(unique_ptr<Process, Deleter>(forward<Args>(args)...)) {}

  template <typename... Args>
  static process_container make(Args&&... args) {
    return make_unique<Process>(forward<Args>(args)...);
  }

  ~process_container() {
    auto process = this->get();
    if (!process) {
      return;
    }
    process->stop();
    assert(this->use_count() == 1);
  }
};

}  // namespace taraxa::util::process_container

#endif  // TARAXA_NODE_UTIL_PROCESS_CONTAINER_HPP_
