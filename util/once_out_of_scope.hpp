#ifndef TARAXA_NODE_UTIL_ONCE_OUT_OF_SCOPE_HPP_
#define TARAXA_NODE_UTIL_ONCE_OUT_OF_SCOPE_HPP_

#include <functional>

namespace taraxa::util::once_out_of_scope {

class OnceOutOfScope {
  std::function<void()> action_;

 public:
  OnceOutOfScope(decltype(action_) const& action) : action_(action) {}
  ~OnceOutOfScope() { action_(); }
};

};      // namespace taraxa::util::once_out_of_scope
#endif  // TARAXA_NODE_UTIL_ONCE_OUT_OF_SCOPE_HPP_
