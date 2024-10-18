#pragma once

#include "common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/packets/latest/dag_block_packet.hpp"

namespace taraxa {
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class DagBlockPacketHandler : public ExtSyncingPacketHandler<DagBlockPacket> {
 public:
  DagBlockPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                        std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                        const addr_t &node_addr, const std::string &logs_prefix = "");

  void sendBlockWithTransactions(dev::p2p::NodeID const &peer_id, DagBlock block, const SharedTransactions &trxs);
  void onNewBlockReceived(DagBlock &&block, const std::shared_ptr<TaraxaPeer> &peer = nullptr,
                          const std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> &trxs = {});
  void onNewBlockVerified(const DagBlock &block, bool proposed, const SharedTransactions &trxs);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kDagBlockPacket;

 private:
  virtual void process(DagBlockPacket &&packet, const std::shared_ptr<TaraxaPeer> &peer) override;

 protected:
  std::shared_ptr<TransactionManager> trx_mgr_{nullptr};
};

}  // namespace taraxa::network::tarcap
