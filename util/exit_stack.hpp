#ifndef TARAXA_NODE_UTIL_EXIT_STACK_HPP_
#define TARAXA_NODE_UTIL_EXIT_STACK_HPP_

#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

namespace taraxa::util::exit_stack {
using namespace std;

class ExitStack {
  vector<function<void()>> actions;

  ExitStack(ExitStack const &) = delete;
  ExitStack &operator=(ExitStack const &) = delete;

 public:
  ExitStack(optional<decltype(actions)::size_type> initial_capacity = nullopt) {
    if (initial_capacity) {
      actions.reserve(*initial_capacity);
    }
  }
  template <typename Action>
  ExitStack(Action &&action,
            optional<decltype(actions)::size_type> initial_capacity = nullopt)
      : ExitStack(initial_capacity) {
    actions.emplace_back(move(action));
  }
  ~ExitStack() {
    for (auto i = actions.rbegin(), end = actions.rend(); i != end; ++i) {
      (*i)();
    }
  }

  template <typename Action,
            typename = enable_if_t<!is_same_v<Action, ExitStack>>>
  ExitStack &operator+=(Action &&action) {
    actions.emplace_back(move(action));
    return *this;
  }
  ExitStack &operator+=(ExitStack &&other) {
    actions.insert(actions.end(), other.actions.begin(), other.actions.end());
    other.actions.clear();
    return *this;
  }
};

};  // namespace taraxa::util::exit_stack

namespace taraxa::util {
using exit_stack::ExitStack;
}

#endif  // TARAXA_NODE_UTIL_EXIT_STACK_HPP_
