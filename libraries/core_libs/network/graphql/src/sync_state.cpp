
#include "graphql/sync_state.hpp"

#include "network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp"

namespace graphql::taraxa {

SyncState::SyncState(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                     std::weak_ptr<::taraxa::Network> network) noexcept
    : final_chain_(std::move(final_chain)), network_(std::move(network)) {}

response::Value SyncState::getStartingBlock() const noexcept { return response::Value(0); }

response::Value SyncState::getCurrentBlock() const noexcept {
  return response::Value(static_cast<int>(final_chain_->last_block_number()));
}

response::Value SyncState::getHighestBlock() const noexcept {
  if (auto net = network_.lock(); net) {
    const auto peer = net->getSpecificHandler<::taraxa::network::tarcap::PbftSyncPacketHandler>()->getMaxChainPeer();
    return response::Value(static_cast<int>(peer->pbft_chain_size_));
  } else {
    return response::Value(0);
  }
}

std::optional<response::Value> SyncState::getPulledStates() const noexcept { return std::nullopt; }

std::optional<response::Value> SyncState::getKnownStates() const noexcept { return std::nullopt; }

}  // namespace graphql::taraxa