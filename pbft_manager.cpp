//
// Created by Qi Gao on 2019-04-10.
//

#include "pbft_manager.h"

#include "full_node.hpp"
#include "sortition.h"

#include <string>

namespace taraxa {

PbftManager::PbftManager(std::shared_ptr<FullNode> node) : node_(node) {}

bool PbftManager::shouldSpeak(blk_hash_t blockhash,
                              char type,
                              int period,
                              int step) {
  std::string message = blockhash.toString() +
                        std::to_string(type) +
                        std::to_string(period) +
                        std::to_string(step);
  dev::Signature signature = node_->signMessage(message);
  string signature_hash = taraxa::hashSignature(signature);
  int account_balance = 1000;
  if (taraxa::sortition(signature_hash, account_balance)) {
    return true;
  } else {
    return false;
  }
}

} // namespace taraxa