#pragma once

#include <memory>
#include <unordered_map>

#include "common/vrf_wrapper.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa {
class KeyManager {
 public:
  KeyManager(std::shared_ptr<FinalChain> final_chain);
  KeyManager(const KeyManager &) = delete;
  KeyManager(KeyManager &&) = delete;
  KeyManager &operator=(const KeyManager &) = delete;
  KeyManager &operator=(KeyManager &&) = delete;

  std::shared_ptr<vrf_wrapper::vrf_pk_t> get(const addr_t &addr, std::optional<EthBlockNumber> blk_n = {});

 private:
  std::shared_mutex mutex_;
  std::unordered_map<addr_t, std::shared_ptr<vrf_wrapper::vrf_pk_t>> key_map_;

  std::shared_ptr<FinalChain> final_chain_;
};
}  // namespace taraxa