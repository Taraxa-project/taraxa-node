#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;
class SyncingHandler;

class DagSyncPacketHandler : public PacketHandler {
 public:
  DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                       std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
                       std::shared_ptr<DagBlockManager> dag_blk_mgr, const addr_t& node_addr = {});

  DagSyncPacketHandler(const DagSyncPacketHandler&) = default;
  DagSyncPacketHandler& operator=(const DagSyncPacketHandler&) = default;
  DagSyncPacketHandler(DagSyncPacketHandler&&) = default;
  DagSyncPacketHandler& operator=(DagSyncPacketHandler&&) = default;
  virtual ~DagSyncPacketHandler() = default;

 private:
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<SyncingHandler> syncing_handler_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
};

}  // namespace taraxa::network::tarcap
