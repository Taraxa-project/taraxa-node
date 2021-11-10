#include "pbft/state_machine.hpp"

namespace taraxa {

// TimeMachine::TimeMachine(addr_t node_addr, uint32_t lambda) : lambda_(lambda) {
//   LOG_OBJECTS_CREATE("TIME_MACHINE");
// }
TimeMachine::TimeMachine(addr_t node_addr) {
  LOG_OBJECTS_CREATE("TIME_MACHINE");
}

TimeMachine::~TimeMachine() { stop(); }

void TimeMachine::stop() {
  std::unique_lock<std::mutex> lock(stop_mtx_);
  stop_cv_.notify_all();
}

void TimeMachine::initialClockInNewRound() {
  round_clock_initial_time_ = std::chrono::system_clock::now();
  // round_clock_initial_datetime_ = now_;
}

void TimeMachine::setTimeOut(time_stamp_t end_time) {
  end_time_ = end_time;
}

void TimeMachine::timeOut() {
//   now_ = std::chrono::system_clock::now();
//   duration_ = now_ - round_clock_initial_datetime_;
//   elapsed_time_in_round_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration_).count();

  auto duration = std::chrono::system_clock::now() - round_clock_initial_time_;
  auto elapsed_time = static_cast<time_stamp_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
  LOG(log_tr_) << "elapsed time in round(ms): " << elapsed_time;

  // Add 25ms for practical reality that a thread will not stall for less than 10-25 ms...
  if (end_time_ > elapsed_time + 25) {
    auto sleep_time = end_time_ - elapsed_time;
    LOG(log_tr_) << "Time to sleep(ms): " << sleep_time;

    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(sleep_time));
  } else {
    LOG(log_tr_) << "Skipping sleep, running late...";
  }
}

}  // namespace taraxa