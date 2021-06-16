#pragma once

#include <chrono>

namespace taraxa::util {

/**
 * @brief Starts time
 *
 * @return std::chrono::steady_clock::time_point
 */
inline std::chrono::steady_clock::time_point startTimer() { return std::chrono::steady_clock::now(); }

/**
 * @brief Stops the timer
 *
 * @tparam DurationType std::chrono::<TYPE>, e.g. std::chrono::microseconds
 * @param begin time obtained by startTimer()
 * @return number of milli/micro/nano/... seconds according to DurationType
 */
template <typename DurationType = std::chrono::milliseconds>
inline uint64_t stopTimer(const std::chrono::steady_clock::time_point& begin) {
  return std::chrono::duration_cast<DurationType>(std::chrono::steady_clock::now() - begin).count();
}

/**
 * Usage example
 *
 * const auto start_time = startTimer();
 *
 * ... some code that should be measured in time
 *
 * auto duration_us = stopTimer(start_time);
 * std::cout << "duration: " << duration_us << " [ms]";
 *
 */

}  // namespace taraxa::util