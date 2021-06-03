#pragma once

#include <boost/exception/diagnostic_information.hpp>
#include <functional>
#include <iostream>
#include <optional>
#include <type_traits>
#include <vector>

namespace taraxa::util::exit_stack {
using namespace std;

class ExitStack {
  vector<function<void()>> actions;

 public:
  ExitStack(ExitStack const &) = delete;
  ExitStack &operator=(ExitStack const &) = delete;

  ExitStack(optional<decltype(actions)::size_type> initial_capacity = nullopt) {
    if (initial_capacity) {
      actions.reserve(*initial_capacity);
    }
  }

  template <typename Action>
  ExitStack(Action &&action, optional<decltype(actions)::size_type> initial_capacity = nullopt)
      : ExitStack(initial_capacity) {
    actions.emplace_back(std::forward<Action>(action));
  }

  ~ExitStack() {
    for (auto i = actions.rbegin(), end = actions.rend(); i != end; ++i) {
      try {
        (*i)();
      } catch (...) {
        cerr << boost::current_exception_diagnostic_information() << endl;
      }
    }
  }

  template <typename Action, typename = enable_if_t<!is_same_v<Action, ExitStack>>>
  ExitStack &operator+=(Action &&action) {
    actions.emplace_back(std::forward<Action>(action));
    return *this;
  }
  ExitStack &operator+=(ExitStack &&other) {
    actions.insert(actions.end(), other.actions.begin(), other.actions.end());
    other.actions.clear();
    return *this;
  }
};

}  // namespace taraxa::util::exit_stack

namespace taraxa::util {
using exit_stack::ExitStack;
}
