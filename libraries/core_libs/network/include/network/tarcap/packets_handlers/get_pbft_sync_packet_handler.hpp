#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftSyncingState;

class GetPbftSyncPacketHandler final : public PacketHandler {
 public:
  GetPbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                           std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                           std::shared_ptr<DbStorage> db, size_t network_sync_level_size, const addr_t& node_addr,
                           bool is_light_node = false, uint64_t light_node_history = 0);

  void sendPbftBlocks(dev::p2p::NodeID const& peer_id, size_t height_to_sync, size_t blocks_to_transfer,
                      bool pbft_chain_synced);

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DbStorage> db_;

  // Initialized from network config
  const size_t network_sync_level_size_;

  const bool is_light_node_ = false;
  const uint64_t light_node_history_ = 0;
};

}  // namespace taraxa::network::tarcap
