#pragma once

#include "common/packet_handler.hpp"
#include "network/tarcap/packets/latest/get_dag_sync_packet.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
class DagManager;
class DbStorage;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class GetDagSyncPacketHandler : public PacketHandler {
 public:
  GetDagSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                          std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                          std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DbStorage> db, const std::string& logs_prefix = "");

  void sendBlocks(const dev::p2p::NodeID& peer_id, std::vector<std::shared_ptr<DagBlock>>&& blocks,
                  SharedTransactions&& transactions, PbftPeriod request_period, PbftPeriod period);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kGetDagSyncPacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
