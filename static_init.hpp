#ifndef TARAXA_NODE__STATIC_INIT_HPP_
#define TARAXA_NODE__STATIC_INIT_HPP_

#include <libethcore/Precompiled.h>

#include "eth/taraxa_seal_engine.hpp"
#include "util.hpp"

namespace taraxa {

inline void static_init() {
  signal(SIGABRT, abortHandler);
  signal(SIGSEGV, abortHandler);
  signal(SIGILL, abortHandler);
  signal(SIGFPE, abortHandler);
  dev::eth::PrecompiledRegistrar::init();
  eth::taraxa_seal_engine::TaraxaSealEngine::init();
}

}  // namespace taraxa

#endif  // TARAXA_NODE__STATIC_INIT_HPP_
