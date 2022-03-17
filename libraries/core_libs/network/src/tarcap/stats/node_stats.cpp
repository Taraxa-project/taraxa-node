#include "network/tarcap/stats/node_stats.hpp"

#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
#include "libp2p/Common.h"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "network/tarcap/stats/packets_stats.hpp"
#include "network/tarcap/threadpool/tarcap_thread_pool.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

NodeStats::NodeStats(std::shared_ptr<PeersState> peers_state, std::shared_ptr<SyncingState> syncing_state,
                     std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                     std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                     std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<const TarcapThreadPool> thread_pool,
                     const addr_t &node_addr)
    : peers_state_(std::move(peers_state)),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      packets_stats_(std::move(packets_stats)),
      thread_pool_(std::move(thread_pool)) {
  LOG_OBJECTS_CREATE("SUMMARY");
}

uint64_t NodeStats::syncTimeSeconds() const { return syncing_duration_seconds; }

void NodeStats::logNodeStats() {
  bool is_syncing = syncing_state_->is_syncing();

  dev::p2p::NodeID max_pbft_round_node_id;
  dev::p2p::NodeID max_pbft_chain_node_id;
  dev::p2p::NodeID max_node_dag_level_node_id;
  uint64_t peer_max_pbft_round = 1;
  uint64_t peer_max_pbft_chain_size = 1;
  uint64_t peer_max_node_dag_level = 1;

  const size_t peers_size = peers_state_->getPeersCount();
  std::string connected_peers_str{""};

  for (auto const &peer : peers_state_->getAllPeers()) {
    // Find max pbft chain size
    if (peer.second->pbft_chain_size_ > peer_max_pbft_chain_size) {
      peer_max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_node_id = peer.first;
    }

    // Find max dag level
    if (peer.second->dag_level_ > peer_max_node_dag_level) {
      peer_max_node_dag_level = peer.second->dag_level_;
      max_node_dag_level_node_id = peer.first;
    }

    // Find max peer PBFT round
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      max_pbft_round_node_id = peer.first;
    }

    connected_peers_str += peer.first.abridged() + " ";
  }

  // Local dag info...
  const auto local_max_level_in_dag = dag_mgr_->getMaxLevel();
  const auto local_max_dag_level_in_queue = dag_blk_mgr_->getMaxDagLevelInQueue();

  // Local pbft info...
  uint64_t local_pbft_round = pbft_mgr_->getPbftRound();
  const auto local_chain_size = pbft_chain_->getPbftChainSize();
  const auto local_chain_size_without_empty_blocks = pbft_chain_->getPbftChainSizeExcludingEmptyPbftBlocks();

  const auto local_dpos_total_votes_count = pbft_mgr_->getDposTotalVotesCount();
  const auto local_dpos_total_address_count = pbft_mgr_->getDposTotalAddressCount();
  const auto local_weighted_votes = pbft_mgr_->getDposWeightedVotesCount();
  const auto local_twotplusone = pbft_mgr_->getTwoTPlusOne();

  // Syncing period...
  const auto local_pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();

  // Decide if making progress...
  const auto pbft_consensus_rounds_advanced = local_pbft_round - local_pbft_round_prev_interval_;
  const auto pbft_chain_size_growth = local_chain_size - local_chain_size_prev_interval_;
  const auto pbft_sync_period_progress = local_pbft_sync_period - local_pbft_sync_period_prev_interval_;
  const auto dag_level_growh = local_max_level_in_dag - local_max_level_in_dag_prev_interval_;

  const bool making_pbft_consensus_progress = (pbft_consensus_rounds_advanced > 0);
  const bool making_pbft_chain_progress = (pbft_chain_size_growth > 0);
  const bool making_pbft_sync_period_progress = (pbft_sync_period_progress > 0);
  const bool making_dag_progress = (dag_level_growh > 0);

  // Update syncing interval counts
  static auto previous_call_time = std::chrono::steady_clock::now();
  auto current_call_time = std::chrono::steady_clock::now();
  uint64_t delta = std::chrono::duration_cast<std::chrono::seconds>(current_call_time - previous_call_time).count();

  if (is_syncing) {
    intervals_syncing_since_launch_++;

    syncing_duration_seconds += delta;
    stalled_syncing_duration_seconds =
        (!making_pbft_chain_progress && !making_dag_progress) ? stalled_syncing_duration_seconds + delta : 0;
  } else {
    intervals_in_sync_since_launch_++;

    syncing_duration_seconds = 0;
    stalled_syncing_duration_seconds = 0;
  }
  previous_call_time = current_call_time;

  LOG(log_dg_) << "Making PBFT chain progress: " << std::boolalpha << making_pbft_chain_progress << " (advanced "
               << pbft_chain_size_growth << " blocks)";
  if (is_syncing) {
    LOG(log_dg_) << "Making PBFT sync period progress: " << std::boolalpha << making_pbft_sync_period_progress
                 << " (synced " << pbft_sync_period_progress << " blocks)";
  }
  LOG(log_dg_) << "Making PBFT consensus progress: " << std::boolalpha << making_pbft_consensus_progress
               << " (advanced " << pbft_consensus_rounds_advanced << " rounds)";
  LOG(log_dg_) << "Making DAG progress: " << std::boolalpha << making_dag_progress << " (grew " << dag_level_growh
               << " dag levels)";

  LOG(log_nf_) << "Connected to " << peers_size << " peers: [ " << connected_peers_str << "]";

  if (is_syncing) {
    // Syncing...
    const auto percent_synced = (local_pbft_sync_period * 100) / peer_max_pbft_chain_size;
    const auto syncing_time_sec = syncTimeSeconds();
    LOG(log_nf_) << "Syncing for " << syncing_time_sec << " seconds, " << percent_synced << "% synced";
    LOG(log_nf_) << "Currently syncing from node " << syncing_state_->syncing_peer();
    LOG(log_nf_) << "Max peer PBFT chain size:       " << peer_max_pbft_chain_size << " (peer "
                 << max_pbft_chain_node_id << ")";
    LOG(log_nf_) << "Max peer PBFT consensus round:  " << peer_max_pbft_round << " (peer " << max_pbft_round_node_id
                 << ")";
    LOG(log_nf_) << "Max peer DAG level:             " << peer_max_node_dag_level << " (peer "
                 << max_node_dag_level_node_id << ")";
  } else {
    const auto sync_percentage =
        (100 * intervals_in_sync_since_launch_) / (intervals_in_sync_since_launch_ + intervals_syncing_since_launch_);
    LOG(log_nf_) << "In sync since launch for " << sync_percentage << "% of the time";
  }
  LOG(log_nf_) << "Max DAG block level in DAG:      " << local_max_level_in_dag;
  LOG(log_nf_) << "Max DAG block level in queue:    " << local_max_dag_level_in_queue;
  LOG(log_nf_) << "PBFT chain size:                 " << local_chain_size << " ("
               << local_chain_size_without_empty_blocks << ")";
  LOG(log_nf_) << "Current PBFT round:              " << local_pbft_round;
  LOG(log_nf_) << "DPOS total votes count:          " << local_dpos_total_votes_count;
  LOG(log_nf_) << "DPOS total addresses count:      " << local_dpos_total_address_count;
  LOG(log_nf_) << "PBFT consensus 2t+1 threshold:   " << local_twotplusone;
  LOG(log_nf_) << "Node elligible vote count:       " << local_weighted_votes;

  LOG(log_dg_) << "****** Memory structures sizes ******";
  LOG(log_dg_) << "Unverified votes size:           " << vote_mgr_->getUnverifiedVotesSize();
  LOG(log_dg_) << "Verified votes size:             " << vote_mgr_->getVerifiedVotesSize();

  LOG(log_dg_) << "Non finalized txs size:          " << trx_mgr_->getNonfinalizedTrxSize();
  LOG(log_dg_) << "Txs pool size:                   " << trx_mgr_->getTransactionPoolSize();

  const auto [non_finalized_blocks_levels, non_finalized_blocks_size] = dag_mgr_->getNonFinalizedBlocksSize();
  LOG(log_dg_) << "Dag blocks size:        " << dag_blk_mgr_->getDagBlockQueueSize();
  LOG(log_dg_) << "Non finalized dag blocks levels: " << non_finalized_blocks_levels;
  LOG(log_dg_) << "Non finalized dag blocks size:   " << non_finalized_blocks_size;

  const auto [high_priority_queue_size, mid_priority_queue_size, low_priority_queue_size] =
      thread_pool_->getQueueSize();
  LOG(log_dg_) << "High priority queue size: " << high_priority_queue_size;
  LOG(log_dg_) << "Mid priority queue size: " << mid_priority_queue_size;
  LOG(log_dg_) << "Low priority queue size: " << low_priority_queue_size;

  LOG(log_nf_) << "------------- tl;dr -------------";

  if (making_pbft_chain_progress) {
    if (is_syncing) {
      LOG(log_nf_) << "STATUS: GOOD. ACTIVELY SYNCING";
    } else if (local_weighted_votes) {
      LOG(log_nf_) << "STATUS: GOOD. NODE SYNCED AND PARTICIPATING IN CONSENSUS";
    } else {
      LOG(log_nf_) << "STATUS: GOOD. NODE SYNCED";
    }
  } else if (is_syncing && (making_pbft_sync_period_progress || making_dag_progress)) {
    LOG(log_nf_) << "STATUS: PENDING SYNCED DATA";
  } else if (!is_syncing && making_pbft_consensus_progress) {
    if (local_weighted_votes) {
      LOG(log_nf_) << "STATUS: PARTICIPATING IN CONSENSUS BUT NO NEW FINALIZED BLOCKS";
    } else {
      LOG(log_nf_) << "STATUS: NODE SYNCED BUT NO NEW FINALIZED BLOCKS";
    }
  } else if (!is_syncing && making_dag_progress) {
    LOG(log_nf_) << "STATUS: PBFT STALLED, POSSIBLY PARTITIONED. NODE HAS NOT RESTARTED SYNCING";
  } else if (peers_size) {
    if (is_syncing) {
      LOG(log_nf_) << "STATUS: SYNCING STALLED. NO PROGRESS MADE IN LAST " << stalled_syncing_duration_seconds
                   << " SECONDS";
    } else {
      LOG(log_nf_) << "STATUS: STUCK. NODE HAS NOT RESTARTED SYNCING";
    }
  } else {
    // Peer size is zero...
    LOG(log_nf_) << "STATUS: NOT CONNECTED TO ANY PEERS. POSSIBLE CONFIG ISSUE OR NETWORK CONNECTIVITY";
  }

  LOG(log_nf_) << "In the last " << delta << " seconds...";

  if (is_syncing) {
    LOG(log_nf_) << "PBFT sync period progress:      " << pbft_sync_period_progress;
  }

  LOG(log_nf_) << "PBFT chain blocks added:        " << pbft_chain_size_growth;
  LOG(log_nf_) << "PBFT rounds advanced:           " << pbft_consensus_rounds_advanced;
  LOG(log_nf_) << "DAG level growth:               " << dag_level_growh;

  LOG(log_nf_) << "##################################";

  // Node stats info history
  local_max_level_in_dag_prev_interval_ = local_max_level_in_dag;
  local_pbft_round_prev_interval_ = local_pbft_round;
  local_chain_size_prev_interval_ = local_chain_size;
  local_pbft_sync_period_prev_interval_ = local_pbft_sync_period;
}

Json::Value NodeStats::getStatus() const {
  Json::Value res;
  dev::p2p::NodeID max_pbft_round_nodeID;
  dev::p2p::NodeID max_pbft_chain_nodeID;
  dev::p2p::NodeID max_node_dag_level_nodeID;
  uint64_t peer_max_pbft_round = 1;
  uint64_t peer_max_pbft_chain_size = 1;
  uint64_t peer_max_node_dag_level = 1;

  res["peers"] = Json::Value(Json::arrayValue);

  for (auto const &peer : peers_state_->getAllPeers()) {
    Json::Value peer_status;
    peer_status["node_id"] = peer.first.toString();
    peer_status["dag_level"] = Json::UInt64(peer.second->dag_level_);
    peer_status["pbft_size"] = Json::UInt64(peer.second->pbft_chain_size_);
    peer_status["dag_synced"] = !peer.second->syncing_;
    res["peers"].append(peer_status);
    // Find max pbft chain size
    if (peer.second->pbft_chain_size_ > peer_max_pbft_chain_size) {
      peer_max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_nodeID = peer.first;
    }

    // Find max dag level
    if (peer.second->dag_level_ > peer_max_node_dag_level) {
      peer_max_node_dag_level = peer.second->dag_level_;
      max_node_dag_level_nodeID = peer.first;
    }

    // Find max peer PBFT round
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      max_pbft_round_nodeID = peer.first;
    }
  }

  if (syncing_state_->is_syncing()) {
    res["syncing_from_node_id"] = syncing_state_->syncing_peer().toString();
  }

  res["peer_max_pbft_round"] = Json::UInt64(peer_max_pbft_round);
  res["peer_max_pbft_chain_size"] = Json::UInt64(peer_max_pbft_chain_size);
  res["peer_max_node_dag_level"] = Json::UInt64(peer_max_node_dag_level);
  res["peer_max_pbft_round_node_id"] = max_pbft_round_nodeID.toString();
  res["peer_max_pbft_chain_size_node_id"] = max_pbft_chain_nodeID.toString();
  res["peer_max_node_dag_level_node_id"] = max_node_dag_level_nodeID.toString();

  return res;
}

Json::Value NodeStats::getPacketsStats() const {
  Json::Value ret;
  ret["received_packets"] = packets_stats_->getReceivedPacketsStats().getStatsJson();
  ret["sent_packets"] = packets_stats_->getSentPacketsStats().getStatsJson();

  ret["received_packets_period_max_stats"] = packets_stats_->getReceivedPacketsStats().getPeriodMaxStatsJson();
  ret["sent_packets_period_max_stats"] = packets_stats_->getSentPacketsStats().getPeriodMaxStatsJson();

  return ret;
}

}  // namespace taraxa::network::tarcap