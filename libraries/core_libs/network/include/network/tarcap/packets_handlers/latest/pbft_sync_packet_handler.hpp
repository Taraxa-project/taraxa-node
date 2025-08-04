#pragma once

#include "common/thread_pool.hpp"
#include "network/tarcap/packets/latest/pbft_sync_packet.hpp"
#include "network/tarcap/packets_handlers/interface/sync_packet_handler.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

class PbftSyncPacketHandler : public ISyncPacketHandler {
 public:
  PbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                        std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db, const addr_t& node_addr,
                        const std::string& logs_prefix = "");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kPbftSyncPacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  void pbftSyncComplete();
  void delayedPbftSync(uint32_t counter);

  static constexpr uint32_t kDelayedPbftSyncDelayMs = 10;

  std::shared_ptr<VoteManager> vote_mgr_;
  util::ThreadPool periodic_events_tp_;
};

}  // namespace taraxa::network::tarcap
