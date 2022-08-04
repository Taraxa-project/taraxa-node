#include "logger/logger.hpp"

namespace taraxa::logger {
struct SharedLogger {
  SharedLogger(const std::string& channel, const addr_t& node_addr) { LOG_OBJECTS_CREATE(channel); }

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa::logger