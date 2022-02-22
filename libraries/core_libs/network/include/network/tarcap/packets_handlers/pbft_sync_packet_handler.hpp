#pragma once

#include "common/thread_pool.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

namespace taraxa::network::tarcap {

class PbftSyncPacketHandler : public ExtSyncingPacketHandler {
 public:
  PbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                        std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                        size_t network_sync_level_size, const addr_t& node_addr);

  virtual ~PbftSyncPacketHandler() = default;

  void handleMaliciousSyncPeer(dev::p2p::NodeID const& id);

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  void pbftSyncComplete();
  void delayedPbftSync(int counter);

  // Initialized from network config
  const size_t network_sync_level_size_;

  // TODO: refactor this: we could have some shared global threadpool for periodic events ?
  // TODO: or std::async with sleep inside could be used instead ?
  // Fake threadpool (1 thread) for delayed syncing events
  util::ThreadPool delayed_sync_events_tp_;
};

}  // namespace taraxa::network::tarcap
