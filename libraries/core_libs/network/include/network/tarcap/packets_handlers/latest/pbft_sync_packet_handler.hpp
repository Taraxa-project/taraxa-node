#pragma once

#include "common/ext_syncing_packet_handler.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

class PbftSyncPacketHandler : public ExtSyncingPacketHandler {
 public:
  PbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                        std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db, const addr_t& node_addr,
                        const std::string& log_channel_name = "PBFT_SYNC_PH");

  void handleMaliciousSyncPeer(const dev::p2p::NodeID& id);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PbftSyncPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  virtual PeriodData decodePeriodData(const dev::RLP& period_data_rlp) const;
  virtual std::vector<std::shared_ptr<Vote>> decodeVotesBundle(const dev::RLP& votes_bundle_rlp) const;

  void pbftSyncComplete();
  void delayedPbftSync(int counter);

  std::shared_ptr<VoteManager> vote_mgr_;
  util::ThreadPool periodic_events_tp_;

  static constexpr size_t kStandardPacketSize = 2;
  static constexpr size_t kChainSyncedPacketSize = 3;
};

}  // namespace taraxa::network::tarcap
