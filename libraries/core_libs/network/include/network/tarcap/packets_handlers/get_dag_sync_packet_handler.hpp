#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class DagManager;
class DagBlockManager;
class DbStorage;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class GetDagSyncPacketHandler : public PacketHandler {
 public:
  GetDagSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                          std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                          const addr_t& node_addr);

  virtual ~GetDagSyncPacketHandler() = default;

  void sendBlocks(const dev::p2p::NodeID& peer_id, std::vector<std::shared_ptr<DagBlock>>&& blocks,
                  SharedTransactions&& transactions, uint64_t request_period, uint64_t period);

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
