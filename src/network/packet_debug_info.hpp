#pragma once

#include <ostream>

namespace taraxa {

/**
 * @class PacketDebugInfo
 * @brief Interface used for debugging purposes, derive your custom class from PacketDebugInfo, save any data
 *        in it and provide debugInfoToString() function, which is used for logging
 */
class PacketDebugInfo {
 public:
  virtual std::string debugInfoToString() const = 0;

  friend std::ostream &operator<<(std::ostream &os, const PacketDebugInfo &debug_info) {
    os << debug_info.debugInfoToString();
    return os;
  }
};

}  // namespace taraxa
