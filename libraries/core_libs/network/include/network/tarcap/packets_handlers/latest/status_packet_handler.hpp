#pragma once

#include "network/tarcap/packets_handlers/interface/sync_packet_handler.hpp"

namespace taraxa::network::tarcap {

class StatusPacketHandler : public ISyncPacketHandler {
 public:
  StatusPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                      std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                      std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                      std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                      std::shared_ptr<DbStorage> db, h256 genesis_hash, const addr_t& node_addr,
                      const std::string& logs_prefix = "");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kStatusPacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  const h256 kGenesisHash;
};

}  // namespace taraxa::network::tarcap
