#pragma once

#include "network/tarcap/packets/v4/dag_sync_packet.hpp"
#include "network/tarcap/packets_handlers/latest/common/ext_syncing_packet_handler.hpp"

namespace taraxa {
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap::v4 {

class DagSyncPacketHandler : public ExtSyncingPacketHandler<v4::DagSyncPacket> {
 public:
  DagSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                       std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                       std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                       std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                       std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                       const addr_t& node_addr, const std::string& logs_prefix = "");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kDagSyncPacket;

 private:
  virtual void process(DagSyncPacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  std::shared_ptr<TransactionManager> trx_mgr_{nullptr};
};

}  // namespace taraxa::network::tarcap::v4
