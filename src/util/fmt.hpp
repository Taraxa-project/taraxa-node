#pragma once

#include <boost/format.hpp>

namespace taraxa::util {

template <typename... TS>
std::string fmt(std::string const &pattern, TS const &... args) {
  return (boost::format(pattern) % ... % args).str();
}

}  // namespace taraxa::util