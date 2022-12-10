#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class DagManager;
class DbStorage;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class GetDagSyncPacketHandler final : public PacketHandler {
 public:
  GetDagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                          std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                          std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DbStorage> db, const addr_t& node_addr);

  void sendBlocks(const dev::p2p::NodeID& peer_id, std::vector<std::shared_ptr<DagBlock>>&& blocks,
                  SharedTransactions&& transactions, PbftPeriod request_period, PbftPeriod period);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetDagSyncPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
