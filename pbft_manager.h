//
// Created by Qi Gao on 2019-04-10.
//

#ifndef PBFT_MANAGER_H
#define PBFT_MANAGER_H

#include "types.hpp"
#include <string>

namespace taraxa {
class FullNode;

class PbftManager {
 public:
  PbftManager(std::weak_ptr<FullNode> node);
  bool shouldSpeak(blk_hash_t blockhash, char type, int period, int step);

 private:
  std::weak_ptr<FullNode> node_;
};

} // namespace taraxa

#endif  // PBFT_MANAGER_H
