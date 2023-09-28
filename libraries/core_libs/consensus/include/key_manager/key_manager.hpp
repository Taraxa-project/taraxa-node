#pragma once

#include <libff/algebra/curves/alt_bn128/alt_bn128_g2.hpp>
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

  std::shared_ptr<vrf_wrapper::vrf_pk_t> getVrfKey(EthBlockNumber blk_n, const addr_t &addr);
  std::shared_ptr<libff::alt_bn128_G2> getBlsKey(EthBlockNumber blk_n, const addr_t &addr);

 private:
  std::shared_mutex vrf_keys_mutex_;
  std::unordered_map<addr_t, std::shared_ptr<vrf_wrapper::vrf_pk_t>> vrf_keys_;

  std::shared_mutex bls_keys_mutex_;
  std::unordered_map<addr_t, std::shared_ptr<libff::alt_bn128_G2>> bls_keys_;

  std::shared_ptr<FinalChain> final_chain_;
};

}  // namespace taraxa