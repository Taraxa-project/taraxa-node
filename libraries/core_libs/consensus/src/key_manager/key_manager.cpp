#include "key_manager/key_manager.hpp"

namespace taraxa {

static const vrf_wrapper::vrf_pk_t kEmptyVrfKey;
static const libff::alt_bn128_G2 kEmptyBlsKey;

KeyManager::KeyManager(std::shared_ptr<FinalChain> final_chain) : final_chain_(std::move(final_chain)) {}

std::shared_ptr<vrf_wrapper::vrf_pk_t> KeyManager::getVrfKey(EthBlockNumber blk_n, const addr_t& addr) {
  {
    std::shared_lock lock(vrf_keys_mutex_);
    if (const auto it = vrf_keys_.find(addr); it != vrf_keys_.end()) {
      return it->second;
    }
  }

  try {
    if (auto key = final_chain_->dpos_get_vrf_key(blk_n, addr); key != kEmptyVrfKey) {
      std::unique_lock lock(vrf_keys_mutex_);
      return vrf_keys_.insert_or_assign(addr, std::make_shared<vrf_wrapper::vrf_pk_t>(std::move(key))).first->second;
    }
  } catch (state_api::ErrFutureBlock&) {
    return nullptr;
  }

  return nullptr;
}

std::shared_ptr<libff::alt_bn128_G2> KeyManager::getBlsKey(EthBlockNumber blk_n, const addr_t& addr) {
  {
    std::shared_lock lock(bls_keys_mutex_);
    if (const auto it = bls_keys_.find(addr); it != bls_keys_.end()) {
      return it->second;
    }
  }

  try {
    // TODO: does kEmptyBlsKey comparison work ???
    if (auto key = final_chain_->dpos_get_bls_key(blk_n, addr); key != kEmptyBlsKey) {
      std::unique_lock lock(bls_keys_mutex_);
      return bls_keys_.insert_or_assign(addr, std::make_shared<libff::alt_bn128_G2>(std::move(key))).first->second;
    }
  } catch (state_api::ErrFutureBlock&) {
    return nullptr;
  }

  return nullptr;
}

}  // namespace taraxa