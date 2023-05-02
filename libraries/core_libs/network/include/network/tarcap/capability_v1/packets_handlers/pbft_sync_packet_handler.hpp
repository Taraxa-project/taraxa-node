#pragma once

#include "consensus/include/vote_manager/vote_manager.hpp"
#include "network/tarcap/capability_latest/packets_handlers/common/ext_syncing_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

class PbftSyncPacketHandler final : public tarcap::ExtSyncingPacketHandler {
 public:
  PbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<tarcap::PeersState> peers_state,
                        std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<tarcap::PbftSyncingState> pbft_syncing_state,
                        std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                        std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<VoteManager> vote_mgr,
                        std::shared_ptr<DbStorage> db, const addr_t& node_addr);

  void handleMaliciousSyncPeer(dev::p2p::NodeID const& id);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PbftSyncPacket;

 private:
  void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  void process(const threadpool::PacketData& packet_data, const std::shared_ptr<tarcap::TaraxaPeer>& peer) override;

  void pbftSyncComplete();
  void delayedPbftSync(int counter);

  std::shared_ptr<VoteManager> vote_mgr_;
  util::ThreadPool periodic_events_tp_;

  static constexpr size_t kStandardPacketSize = 2;
  static constexpr size_t kChainSyncedPacketSize = 3;
};

}  // namespace taraxa::network::tarcap::v1
