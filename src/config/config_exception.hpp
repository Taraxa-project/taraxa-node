#ifndef TARAXA_NODE_CONFIG_EXCEPTION_HPP
#define TARAXA_NODE_CONFIG_EXCEPTION_HPP

#include <exception>

namespace taraxa {

struct ConfigException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

}  // namespace taraxa

#endif  // TARAXA_NODE_CONFIG_EXCEPTION_HPP
