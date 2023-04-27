#pragma once

#include "common/ext_syncing_packet_handler.hpp"
#include "consensus/include/vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

class PbftSyncPacketHandler final : public ExtSyncingPacketHandler {
 public:
  PbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                        std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<util::ThreadPool> periodic_events_tp,
                        std::shared_ptr<DbStorage> db, const addr_t& node_addr);

  void handleMaliciousSyncPeer(const dev::p2p::NodeID& id);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PbftSyncPacket;

 private:
  void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  void pbftSyncComplete();
  void delayedPbftSync(int counter);

  std::shared_ptr<VoteManager> vote_mgr_;

  std::weak_ptr<util::ThreadPool> periodic_events_tp_;

  static constexpr size_t kStandardPacketSize = 2;
  static constexpr size_t kChainSyncedPacketSize = 3;
};

}  // namespace taraxa::network::tarcap
