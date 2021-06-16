#pragma once

#include <exception>

namespace taraxa {

struct ConfigException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

}  // namespace taraxa
