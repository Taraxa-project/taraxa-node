#include "key_manager/key_manager.hpp"

namespace taraxa {

static const vrf_wrapper::vrf_pk_t kEmptyVrfKey;

KeyManager::KeyManager(std::shared_ptr<final_chain::FinalChain> final_chain) : final_chain_(std::move(final_chain)) {}

std::shared_ptr<vrf_wrapper::vrf_pk_t> KeyManager::getVrfKey(EthBlockNumber blk_n, const addr_t& addr) {
  {
    std::shared_lock lock(vrf_keys_mutex_);
    if (const auto it = vrf_keys_.find(addr); it != vrf_keys_.end()) {
      return it->second;
    }
  }

  try {
    if (auto key = final_chain_->dposGetVrfKey(blk_n, addr); key != kEmptyVrfKey) {
      std::unique_lock lock(vrf_keys_mutex_);
      return vrf_keys_.insert_or_assign(addr, std::make_shared<vrf_wrapper::vrf_pk_t>(std::move(key))).first->second;
    }
  } catch (state_api::ErrFutureBlock&) {
    return nullptr;
  }

  return nullptr;
}

}  // namespace taraxa