#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"
#include "util/thread_pool.hpp"

namespace taraxa {
class PbftChain;
class DagBlockManager;
class PbftManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;
class SyncingHandler;

class PbftBlockPacketHandler : public PacketHandler {
 public:
  PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                         std::shared_ptr<DagBlockManager> dag_blk_mgr, size_t network_sync_level_size,
                         const addr_t& node_addr = {});

  virtual ~PbftBlockPacketHandler() = default;

  void sendSyncedMessage();

 private:
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  void pbftSyncComplete();
  void delayedPbftSync(int counter);

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<SyncingHandler> syncing_handler_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;

  // Initialized from network config
  const size_t network_sync_level_size_;

  // TODO: refactor this: we could have some shared global threadpool for periodic events ?
  // TODO: or std::async with sleep inside could be used instead ?
  // Fake threadpool (1 thread) for delayed syncing events
  util::ThreadPool delayed_sync_events_tp_;
};

}  // namespace taraxa::network::tarcap
