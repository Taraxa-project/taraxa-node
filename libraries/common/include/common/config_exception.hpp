#pragma once

#include <stdexcept>

namespace taraxa {

struct ConfigException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

}  // namespace taraxa
