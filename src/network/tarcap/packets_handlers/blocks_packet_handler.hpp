#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;
class SyncingHandler;

class BlocksPacketHandler : public PacketHandler {
 public:
  BlocksPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                      std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
                      std::shared_ptr<DagBlockManager> dag_blk_mgr, const addr_t& node_addr = {});

  BlocksPacketHandler(const BlocksPacketHandler&) = default;
  BlocksPacketHandler& operator=(const BlocksPacketHandler&) = default;
  BlocksPacketHandler(BlocksPacketHandler&&) = default;
  BlocksPacketHandler& operator=(BlocksPacketHandler&&) = default;
  virtual ~BlocksPacketHandler() = default;

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data,
               const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<SyncingHandler> syncing_handler_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
};

}  // namespace taraxa::network::tarcap