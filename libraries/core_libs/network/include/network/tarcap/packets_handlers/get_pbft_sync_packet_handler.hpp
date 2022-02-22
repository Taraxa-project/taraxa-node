#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

class GetPbftSyncPacketHandler : public PacketHandler {
 public:
  GetPbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                           std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                           std::shared_ptr<DbStorage> db, size_t network_sync_level_size, const addr_t& node_addr);

  virtual ~GetPbftSyncPacketHandler() = default;

  void sendPbftBlocks(dev::p2p::NodeID const& peer_id, size_t height_to_sync, size_t blocks_to_transfer);

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DbStorage> db_;

  // Initialized from network config
  const size_t network_sync_level_size_;
};

}  // namespace taraxa::network::tarcap
