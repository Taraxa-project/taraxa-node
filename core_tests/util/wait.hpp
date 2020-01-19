#ifndef TARAXA_NODE_CORE_TESTS_UTIL_WAIT_HPP_
#define TARAXA_NODE_CORE_TESTS_UTIL_WAIT_HPP_

#include <chrono>
#include <optional>
#include <thread>

namespace taraxa::core_tests::util::wait {
using namespace dev;
using namespace std;
using namespace std::chrono;

struct WaitOptions {
  uint attempts;
  nanoseconds backoff;
};

inline const WaitOptions WaitOptions_DEFAULT = {
    60 * 8,
    nanoseconds(1000 * 1000 * 1000 * 1),
};

template <typename Condition>
bool wait(Condition const& condition,
          WaitOptions const& opts = WaitOptions_DEFAULT) {
  for (uint i(0); i < opts.attempts; ++i) {
    if (condition()) {
      return true;
    }
    this_thread::sleep_for(opts.backoff);
  }
  return false;
}

}  // namespace taraxa::core_tests::util::wait

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_WAIT_HPP_
