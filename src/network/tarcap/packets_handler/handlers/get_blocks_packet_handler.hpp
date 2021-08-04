#pragma once

#include "network/tarcap/packets_handler/handlers/common/packet_handler.hpp"

namespace taraxa {
class DagManager;
class DbStorage;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class GetBlocksPacketsHandler : public PacketHandler {
 public:
  GetBlocksPacketsHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                          std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DbStorage> db, const addr_t &node_addr = {});

  void sendBlocks(dev::p2p::NodeID const &peer_id, std::vector<std::shared_ptr<DagBlock>> blocks);

 private:
  void process(const PacketData &packet_data, const dev::RLP &packet_rlp) override;

  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap