#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

class PbftBlockPacketHandler : public PacketHandler {
 public:
  PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<SyncingState> syncing_state,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                         const addr_t &node_addr = {});

  void sendSyncedMessage();

 private:
  void process(const PacketData &packet_data, const dev::RLP &packet_rlp) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
};

}  // namespace taraxa::network::tarcap