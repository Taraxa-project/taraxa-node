#pragma once

#include "common/types.hpp"
#include "json/value.h"
#include "logger/logger.hpp"

namespace taraxa {
class PbftChain;
class PbftManager;
class VoteManager;
class DagManager;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PeersState;
class PbftSyncingState;
class AllPacketsStats;
class TarcapThreadPool;

class NodeStats {
 public:
  NodeStats(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PbftSyncingState> pbft_syncing_state,
            std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
            std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<VoteManager> vote_mgr,
            std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<AllPacketsStats> packets_stats,
            std::shared_ptr<const TarcapThreadPool> thread_pool, const addr_t &node_addr);

  void logNodeStats();
  uint64_t syncTimeSeconds() const;

  Json::Value getStatus() const;
  Json::Value getPacketsStats() const;

 private:
  std::shared_ptr<PeersState> peers_state_;
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<AllPacketsStats> packets_stats_;
  std::shared_ptr<const TarcapThreadPool> thread_pool_;

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
