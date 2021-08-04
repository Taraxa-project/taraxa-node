#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;
class SyncingHandler;

class PbftBlockPacketHandler : public PacketHandler {
 public:
  PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                         const addr_t &node_addr = {});

  void sendSyncedMessage();

  //  /**
  //   * @brief This method only calls syncing_handler_->restartSyncingPbft(). It is exposed through
  //   PbftBlockPacketHandler
  //   *        for classes that do not have access to SyncingHandler, e.g. TaraxaCapability
  //   *
  //   * @param force
  //   */
  //  void restartSyncingPbft(bool force);

 private:
  void process(const PacketData &packet_data, const dev::RLP &packet_rlp) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<SyncingHandler> syncing_handler_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
};

}  // namespace taraxa::network::tarcap