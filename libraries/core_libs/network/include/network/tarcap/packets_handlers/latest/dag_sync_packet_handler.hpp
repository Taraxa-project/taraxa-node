#pragma once

#include "common/ext_syncing_packet_handler.hpp"

namespace taraxa {
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class DagSyncPacketHandler : public ExtSyncingPacketHandler {
 public:
  DagSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                       std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                       std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                       std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                       std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                       const addr_t& node_addr);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::DagSyncPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  std::shared_ptr<TransactionManager> trx_mgr_{nullptr};
};

}  // namespace taraxa::network::tarcap
