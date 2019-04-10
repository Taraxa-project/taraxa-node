//
// Created by Qi Gao on 2019-04-10.
//

#include "pbft_manager.h"

#include "full_node.hpp"
#include "sortition.h"

#include <string>

namespace taraxa {

PbftManager::PbftManager(std::weak_ptr<FullNode> node) : node_(node) {}

bool PbftManager::shouldSpeak(blk_hash_t blockhash,
                              char type,
                              int period,
                              int step) {
  std::string message = blockhash.toString() +
                        std::to_string(type) +
                        std::to_string(period) +
                        std::to_string(step);
  auto full_node  = node_.lock();
  if (!full_node) {
    std::cerr << "PBFT manager full node weak pointer empty" << std::endl;
    return false;
  }
  dev::Signature signature = full_node->signMessage(message);
  string signature_hash = taraxa::hashSignature(signature);
  std::pair<bal_t, bool> account_balance =
      full_node->getBalance(full_node->getAddress());
  if (!account_balance.second) {
    return false;
  }
  if (taraxa::sortition(signature_hash, account_balance.first)) {
    return true;
  } else {
    return false;
  }
}

} // namespace taraxa