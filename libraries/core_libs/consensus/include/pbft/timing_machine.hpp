#pragma once

// #include "common/types.hpp"
#include "logger/logger.hpp"

namespace taraxa {

/**
 * All players keep a timer clock. The timer clock will reset to 0 at every new round. That don’t require all players
 * clock to be synchronized, only require they have same clock speed.
 **/
class TimingMachine {
 public:
  //   TimingMachine(addr_t node_addr, uint32_t lambda);
  TimingMachine(addr_t node_addr);
  ~TimingMachine();

  void stop();

  void initialClockInNewRound();
  void setTimeOut(time_stamp_t end_time);
  void timeOut();
  void wakeUp();

 private:
  //   time_stamp_t lambda_;

  //   time_point_t round_clock_initial_datetime_;
  //   time_point_t now_;
  //   time_stamp_t next_step_time_ms_ = 0;
  time_point_t round_clock_initial_time_;
  time_stamp_t end_time_ = 0;
  //   time_stamp_t elapsed_time_in_round_ms_ = 0;
  //   std::chrono::duration<double> duration_;

  // std::condition_variable stop_cv_;
  std::condition_variable sleep_cv_;
  std::mutex stop_mtx_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa