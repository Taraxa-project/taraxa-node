#include "key_manager/key_manager.hpp"

namespace taraxa {

static const vrf_wrapper::vrf_pk_t kEmptyVrfKey;

KeyManager::KeyManager(std::shared_ptr<FinalChain> final_chain) : final_chain_(std::move(final_chain)) {}

std::shared_ptr<vrf_wrapper::vrf_pk_t> KeyManager::get(EthBlockNumber blk_n, const addr_t& addr) {
  {
    std::shared_lock lock(mutex_);
    if (const auto it = key_map_.find(addr); it != key_map_.end()) {
      return it->second;
    }
  }

  try {
    if (auto key = final_chain_->dpos_get_vrf_key(blk_n, addr); key != kEmptyVrfKey) {
      std::unique_lock lock(mutex_);
      return key_map_.insert_or_assign(addr, std::make_shared<vrf_wrapper::vrf_pk_t>(std::move(key))).first->second;
    }
  } catch (state_api::ErrFutureBlock& e) {
    return nullptr;
  }

  return nullptr;
}
}  // namespace taraxa