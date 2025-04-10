#pragma once

#include "network/tarcap/packets_handlers/latest/common/ext_syncing_packet_handler.hpp"

namespace taraxa::network::tarcap {

class IDagBlockPacketHandler : public ExtSyncingPacketHandler {
 public:
  IDagBlockPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                         std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                         std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                         std::shared_ptr<DbStorage> db, const addr_t &node_addr, const std::string &logs_prefix);

  void onNewBlockVerified(const std::shared_ptr<DagBlock> &block, bool proposed, const SharedTransactions &trxs);
  virtual void sendBlockWithTransactions(const std::shared_ptr<TaraxaPeer> &peer,
                                         const std::shared_ptr<DagBlock> &block, SharedTransactions &&trxs) = 0;

  // Note: Used only in tests
  void requestDagBlocks(std::shared_ptr<TaraxaPeer> peer);
};

}  // namespace taraxa::network::tarcap
