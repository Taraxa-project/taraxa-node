#pragma once

#include "common/types.hpp"
#include "logger/log.hpp"
#include "json/value.h"

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
            std::shared_ptr<PacketsStats> packets_stats, uint64_t stats_log_interval, const addr_t &node_addr = {});
  NodeStats(const NodeStats&) = default;
  NodeStats(NodeStats&&) = default;

  void logNodeStats();

  uint64_t getNodeStatsLogInterval() const;
  Json::Value getStatus() const;
  uint64_t syncTimeSeconds() const;

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
  uint64_t syncing_interval_count_{0};
  uint64_t intervals_in_sync_since_launch_{0};
  uint64_t intervals_syncing_since_launch_{0};
  uint64_t syncing_stalled_interval_count_{0};

  // How often should be node stats logged - called "logNodeStats"
  const uint64_t stats_log_interval_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
