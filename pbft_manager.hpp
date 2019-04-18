//
// Created by Qi Gao on 2019-04-10.
//

#ifndef PBFT_MANAGER_HPP
#define PBFT_MANAGER_HPP

#include <string>
#include <thread>
#include "libdevcore/Log.h"
#include "types.hpp"

namespace taraxa {
class FullNode;

class PbftManager {
 public:
  PbftManager() = default;
  ~PbftManager() { stop(); }
  void setFullNode(std::shared_ptr<FullNode> node) { node_ = node; }
  bool shouldSpeak(blk_hash_t const &blockhash, char type, int period,
                   int step);
  void start();
  void stop();
  void run();

 private:
  bool stopped_ = true;
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<std::thread> executor_;

  mutable dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PBFT_MGR")};
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFT_MGR")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFT_MGR")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_MGR")};
};

}  // namespace taraxa

#endif  // PBFT_MANAGER_H
