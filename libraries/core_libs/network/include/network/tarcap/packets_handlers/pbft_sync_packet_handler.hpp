#pragma once

#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

namespace taraxa::network::tarcap {

class PbftSyncPacketHandler final : public ExtSyncingPacketHandler {
 public:
  PbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                        std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<DagBlockManager> dag_blk_mgr,
                        std::shared_ptr<util::ThreadPool> periodic_events_tp, std::shared_ptr<DbStorage> db,
                        size_t network_sync_level_size, const addr_t& node_addr);

  void handleMaliciousSyncPeer(dev::p2p::NodeID const& id);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PbftSyncPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  void pbftSyncComplete();
  void delayedPbftSync(int counter);

  std::weak_ptr<util::ThreadPool> periodic_events_tp_;

  // Initialized from network config
  const size_t network_sync_level_size_;

  const size_t kStandardPacketSize = 2;
  const size_t kChainSyncedPacketSize = 3;
};

}  // namespace taraxa::network::tarcap
