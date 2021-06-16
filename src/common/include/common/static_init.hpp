#pragma once

#include <sodium/core.h>

#include "util/util.hpp"

namespace taraxa {

inline void static_init() {
  if (sodium_init() == -1) {
    throw std::runtime_error("libsodium init failure");
  }
}

}  // namespace taraxa
