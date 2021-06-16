#include "common/types.hpp"

namespace taraxa {

time_point_t getLong2TimePoint(unsigned long l) {
  std::chrono::duration<unsigned long> dur(l);
  return std::chrono::time_point<std::chrono::system_clock>(dur);
}
unsigned long getTimePoint2Long(time_point_t tp) { return tp.time_since_epoch().count(); }

std::ostream &operator<<(std::ostream &strm, bytes const &bytes) {
  for (auto const &b : bytes) {
    strm << b << " ";
  }
  return strm;
}

}  // namespace taraxa
