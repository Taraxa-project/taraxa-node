//
// Created by Qi Gao on 2019-04-10.
//

#include "pbft_manager.hpp"
#include <string>
#include "full_node.hpp"
#include "sortition.h"
#include "util.hpp"

namespace taraxa {
void PbftManager::start() {
  if (!stopped_) return;
  stopped_ = false;
  executor_ = std::make_shared<std::thread>([this]() { run(); });
  LOG(log_nf_) << "A PBFT executor initiated ..." << std::endl;
}
void PbftManager::stop() {
  if (stopped_) return;
  stopped_ = true;
  executor_->join();
  executor_.reset();
  LOG(log_nf_) << "A PBFT executor terminated ..." << std::endl;
  assert(executor_ == nullptr);
}

void PbftManager::run() {
  uint64_t counter = 0;
  while (!stopped_) {
    // Do Pbft stuff here
    thisThreadSleepForMilliSeconds(500);
    LOG(log_nf_) << "Try to do some pbft stuff ..." << counter++ << std::endl;
  }
}
bool PbftManager::shouldSpeak(blk_hash_t const& blockhash, char type,
                              int period, int step) {
  std::string message = blockhash.toString() + std::to_string(type) +
                        std::to_string(period) + std::to_string(step);
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_er_) << "Full node unavailable" << std::endl;
    return false;
  }
  dev::Signature signature = full_node->signMessage(message);
  string signature_hash = taraxa::hashSignature(signature);
  std::pair<bal_t, bool> account_balance =
      full_node->getBalance(full_node->getAddress());
  if (!account_balance.second) {
    LOG(log_wr_) << "Full node account unavailable" << std::endl;
    return false;
  }
  if (taraxa::sortition(signature_hash, account_balance.first)) {
    LOG(log_nf_) << "Win sortition" << std::endl;
    return true;
  } else {
    return false;
  }
}

}  // namespace taraxa