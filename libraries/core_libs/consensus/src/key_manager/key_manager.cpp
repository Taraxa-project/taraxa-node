#include "key_manager/key_manager.hpp"

namespace taraxa {

static const vrf_wrapper::vrf_pk_t kEmptyVrfKey;

KeyManager::KeyManager(std::shared_ptr<FinalChain> final_chain) : final_chain_(std::move(final_chain)) {}

std::shared_ptr<vrf_wrapper::vrf_pk_t> KeyManager::get(const addr_t& addr) {
  {
    std::shared_lock lock(mutex_);
    if (key_map_.contains(addr)) {
      return key_map_.at(addr);
    }
  }
  {
    std::unique_lock lock(mutex_);
    if (auto key = final_chain_->get_vrf_key(addr); key != kEmptyVrfKey) {
      key_map_[addr] = std::make_shared<vrf_wrapper::vrf_pk_t>(key);
      return key_map_.at(addr);
    } else {
      return nullptr;
    }
  }
}
}  // namespace taraxa