#pragma once

#include "common/ext_syncing_packet_handler.hpp"

namespace taraxa {
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class TestState;

class DagBlockPacketHandler final : public ExtSyncingPacketHandler {
 public:
  DagBlockPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                        std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                        std::shared_ptr<TestState> test_state, const addr_t &node_addr);

  void sendBlock(dev::p2p::NodeID const &peer_id, DagBlock block, const SharedTransactions &trxs);
  void onNewBlockReceived(DagBlock &&block, const std::shared_ptr<TaraxaPeer> &peer = nullptr);
  void onNewBlockVerified(const DagBlock &block, bool proposed, const SharedTransactions &trxs);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::DagBlockPacket;

 private:
  void validatePacketRlpFormat(const threadpool::PacketData &packet_data) const override;
  void process(const threadpool::PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) override;

  std::shared_ptr<TestState> test_state_;
  std::shared_ptr<TransactionManager> trx_mgr_{nullptr};
};

}  // namespace taraxa::network::tarcap
