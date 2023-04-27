
#include "graphql/sync_state.hpp"

#include "network/tarcap/capability_latest/packets_handlers/pbft_sync_packet_handler.hpp"

namespace graphql::taraxa {

SyncState::SyncState(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                     std::weak_ptr<::taraxa::Network> network) noexcept
    : final_chain_(std::move(final_chain)), network_(std::move(network)) {}

response::Value SyncState::getStartingBlock() const noexcept { return response::Value(0); }

response::Value SyncState::getCurrentBlock() const noexcept {
  return response::Value(static_cast<int>(final_chain_->last_block_number()));
}

response::Value SyncState::getHighestBlock() const noexcept {
  auto net = network_.lock();
  if (!net) {
    return {};
  }

  const auto peer = net->getMaxChainPeer();
  if (!peer) {
    return {};
  }

  return response::Value(static_cast<int>(peer->pbft_chain_size_));
}

std::optional<response::Value> SyncState::getPulledStates() const noexcept { return std::nullopt; }

std::optional<response::Value> SyncState::getKnownStates() const noexcept { return std::nullopt; }

}  // namespace graphql::taraxa