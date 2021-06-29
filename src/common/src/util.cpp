#include "common/util.hpp"

#include <cxxabi.h>
#include <dlfcn.h>
#include <errno.h>
namespace taraxa {

void thisThreadSleepForSeconds(unsigned sec) { std::this_thread::sleep_for(std::chrono::seconds(sec)); }

void thisThreadSleepForMilliSeconds(unsigned millisec) {
  std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
}

void thisThreadSleepForMicroSeconds(unsigned microsec) {
  std::this_thread::sleep_for(std::chrono::microseconds(microsec));
}

unsigned long getCurrentTimeMilliSeconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string getFormattedVersion(std::initializer_list<uint32_t> list) {
  std::string ret;
  for (auto elem : list) {
    ret = std::to_string(elem) + ".";
  }
  return ret.substr(0, ret.size() - 1);
}

}  // namespace taraxa
