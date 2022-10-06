#include "key_manager/key_manager.hpp"

namespace taraxa {

static const vrf_wrapper::vrf_pk_t kEmptyVrfKey;

KeyManager::KeyManager(std::shared_ptr<FinalChain> final_chain) : final_chain_(std::move(final_chain)) {}

std::shared_ptr<vrf_wrapper::vrf_pk_t> KeyManager::get(const addr_t& addr, std::optional<EthBlockNumber> blk_n) {
  {
    std::shared_lock lock(mutex_);
    if (const auto it = key_map_.find(addr); it != key_map_.end()) {
      return it->second;
    }
  }
  {
    std::unique_lock lock(mutex_);
    if (auto key = final_chain_->get_vrf_key(addr, std::move(blk_n)); key != kEmptyVrfKey) {
      const auto [it, _] = key_map_.insert_or_assign(addr, std::make_shared<vrf_wrapper::vrf_pk_t>(std::move(key)));
      return it->second;
    }
    return nullptr;
  }
}
}  // namespace taraxa