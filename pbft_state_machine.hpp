#ifndef TARAXA_NODE_PBFT_STATE_MACHINE_HPP
#define TARAXA_NODE_PBFT_STATE_MACHINE_HPP

#include <libdevcore/Log.h>

#include <atomic>
#include <memory>
#include <thread>

#include "types.hpp"

namespace taraxa {
class PbftManager;

class PbftStateMachine {
  friend class PbftManager;

 public:

  PbftStateMachine(PbftManager* pbft_mgr);

  void start();
  void stop();
  void run();

 private:
  PbftManager* const pbft_mgr_;

  std::atomic<bool> stop_ = true;
  std::unique_ptr<std::thread> daemon_;

  // Obtain the next state of pbft_mgr_
  int getNextState();

  mutable dev::Logger log_sil_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PBFT_STATE_MACHINE")};
  mutable dev::Logger log_err_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFT_STATE_MACHINE")};
  mutable dev::Logger log_war_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFT_STATE_MACHINE")};
  mutable dev::Logger log_inf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_STATE_MACHINE")};
  mutable dev::Logger log_deb_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PBFT_STATE_MACHINE")};
  mutable dev::Logger log_tra_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PBFT_STATE_MACHINE")};

  mutable dev::Logger log_inf_test_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_TEST")};
};

}  // namespace taraxa

#endif  // PBFT_STATE_MACHINE_H
