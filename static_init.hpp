#ifndef TARAXA_NODE__STATIC_INIT_HPP_
#define TARAXA_NODE__STATIC_INIT_HPP_

#include "util.hpp"

namespace taraxa {

inline void static_init() {
  signal(SIGABRT, abortHandler);
  signal(SIGSEGV, abortHandler);
  signal(SIGILL, abortHandler);
  signal(SIGFPE, abortHandler);
}

}  // namespace taraxa

#endif  // TARAXA_NODE__STATIC_INIT_HPP_
