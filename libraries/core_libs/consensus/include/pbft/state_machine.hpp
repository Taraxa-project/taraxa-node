#pragma once

#include "common/types.hpp"
#include "logger/logger.hpp"

namespace taraxa {

/**
 * Proposal state: generate PBFT block and propose vote at the block hash

 * Filter state: identify leader block from all received proposed values in the current round by using minimum VRF
 * sortition output. Soft vote at the leader block hash. In filter state, don’t need check voted value correction. This
 * is pre-pre state.

 * Certify state: if receive enough(2t+1) soft vote, cert vote at the value. This is pre state.

 * First finish state: happens at even number steps from step 4. Next vote at finishing value for the current round.
 * This is commit state.

 * Second finish state: happens at odd number steps from step 5. Next vote at finishing value for the current round.
 * This is commit state.
 **/
enum PbftStates : uint8_t { ProposalState = 0, FilterState, CertifyState, FirstFinishState, SecondFinishState };
// enum PbftStates { value_proposal_state = 1, filter_state, certify_state, finish_state, finish_polling_state };

/**
 * All players keep a timer clock. The timer clock will reset to 0 at every new round. That don’t require all players
 * clock to be synchronized, only require they have same clock speed.
 **/
class TimeMachine {
 public:
//   TimeMachine(addr_t node_addr, uint32_t lambda);
  TimeMachine(addr_t node_addr);
  ~TimeMachine();

  void stop();

  void initialClockInNewRound();
  void setTimeOut(time_stamp_t end_time);
  void timeOut();

 private:
//   time_stamp_t lambda_;

//   time_point_t round_clock_initial_datetime_;
//   time_point_t now_;
//   time_stamp_t next_step_time_ms_ = 0;
  time_point_t round_clock_initial_time_;
  time_stamp_t end_time_ = 0;
//   time_stamp_t elapsed_time_in_round_ms_ = 0;
//   std::chrono::duration<double> duration_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa