#pragma once

#include "common/types.hpp"
#include "json/value.h"
#include "logger/log.hpp"

namespace taraxa {
class PbftChain;
class PbftManager;
class VoteManager;
class DagManager;
class DagBlockManager;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PeersState;
class SyncingState;
class PacketsStats;

class NodeStats {
 public:
  NodeStats(std::shared_ptr<PeersState> peers_state, std::shared_ptr<SyncingState> syncing_state,
            std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
            std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
            std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<TransactionManager> trx_mgr,
            std::shared_ptr<PacketsStats> packets_stats, const addr_t &node_addr = {});
  NodeStats(const NodeStats &) = default;
  NodeStats(NodeStats &&) = default;

  void logNodeStats();
  uint64_t syncTimeSeconds() const;

  Json::Value getStatus() const;
  Json::Value getPacketsStats() const;

 private:
  std::shared_ptr<PeersState> peers_state_;
  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<PacketsStats> packets_stats_;

  level_t local_max_level_in_dag_prev_interval_{0};
  uint64_t local_pbft_round_prev_interval_{0};
  uint64_t local_chain_size_prev_interval_{0};
  uint64_t local_pbft_sync_period_prev_interval_{0};
  uint64_t intervals_in_sync_since_launch_{0};
  uint64_t intervals_syncing_since_launch_{0};
  uint64_t syncing_duration_seconds{0};
  uint64_t stalled_syncing_duration_seconds{0};

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
