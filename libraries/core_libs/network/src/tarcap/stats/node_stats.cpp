#include "network/tarcap/stats/node_stats.hpp"

#include "config/version.hpp"
#include "dag/dag_manager.hpp"
#include "libp2p/Common.h"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "network/tarcap/stats/time_period_packets_stats.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "network/threadpool/tarcap_thread_pool.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

NodeStats::NodeStats(std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                     std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                     std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                     std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                     std::shared_ptr<const threadpool::PacketsThreadPool> thread_pool, const FullNodeConfig &config)
    : pbft_syncing_state_(std::move(pbft_syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      packets_stats_(std::move(packets_stats)),
      thread_pool_(std::move(thread_pool)),
      node_addresses_(""),
      logger_(logger::Logging::get().CreateChannelLogger("SUMMARY")) {
  std::for_each(config.wallets.begin(), config.wallets.end(),
                [&](const WalletConfig &wallet) { node_addresses_ += wallet.node_addr.toString() + " "; });
}

uint64_t NodeStats::syncTimeSeconds() const { return syncing_duration_seconds; }

void NodeStats::logNodeStats(const std::vector<std::shared_ptr<network::tarcap::TaraxaPeer>> &all_peers,
                             const std::vector<std::string> &nodes) {
  bool is_pbft_syncing = pbft_syncing_state_->isPbftSyncing();

  dev::p2p::NodeID max_pbft_round_node_id;
  dev::p2p::NodeID max_pbft_chain_node_id;
  dev::p2p::NodeID max_node_dag_level_node_id;
  uint64_t peer_max_pbft_round = 1;
  uint64_t peer_max_pbft_chain_size = 1;
  uint64_t peer_max_node_dag_level = 1;

  const size_t peers_size = all_peers.size();
  std::string connected_peers_str{""};
  std::string connected_peers_str_with_ip{""};

  size_t number_of_discov_peers = nodes.size();
  for (auto const &peer : all_peers) {
    // Find max pbft chain size
    if (peer->pbft_chain_size_ > peer_max_pbft_chain_size) {
      peer_max_pbft_chain_size = peer->pbft_chain_size_;
      max_pbft_chain_node_id = peer->getId();
    }

    // Find max dag level
    if (peer->dag_level_ > peer_max_node_dag_level) {
      peer_max_node_dag_level = peer->dag_level_;
      max_node_dag_level_node_id = peer->getId();
    }

    // Find max peer PBFT round
    if (peer->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer->pbft_round_;
      max_pbft_round_node_id = peer->getId();
    }

    connected_peers_str += peer->getId().abridged() + " ";
    connected_peers_str_with_ip += peer->getId().abridged() + ":" + peer->address_ + " ";
  }

  // Local dag info...
  const auto local_max_level_in_dag = dag_mgr_->getMaxLevel();

  // Local pbft info...
  const auto [local_pbft_round, local_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
  const auto local_chain_size = pbft_chain_->getPbftChainSize();
  const auto local_chain_size_without_empty_blocks = pbft_chain_->getPbftChainSizeExcludingEmptyPbftBlocks();

  const auto local_dpos_total_votes_count = pbft_mgr_->getCurrentDposTotalVotesCount();
  uint64_t local_dpos_node_votes_count = 0;
  if (const auto votes_count = pbft_mgr_->getCurrentNodeVotesCount()) {
    local_dpos_node_votes_count = *votes_count;
  }
  const auto local_twotplusone = vote_mgr_->getPbftTwoTPlusOne(local_pbft_period - 1, PbftVoteTypes::cert_vote);

  // Syncing period...
  const auto local_pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();

  // Decide if making progress...
  const auto pbft_consensus_rounds_advanced =
      (local_pbft_round > local_pbft_round_prev_interval_) ? (local_pbft_round - local_pbft_round_prev_interval_) : 0;
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

  if (is_pbft_syncing) {
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

  logger_->debug("Making PBFT chain progress: {} (advanced {} blocks)", making_pbft_chain_progress,
                 pbft_chain_size_growth);
  if (is_pbft_syncing) {
    logger_->debug("Making PBFT sync period progress: {} (synced {} blocks)", making_pbft_sync_period_progress,
                   pbft_sync_period_progress);
  }
  logger_->debug("Making PBFT consensus progress: {} (advanced {} rounds)", making_pbft_consensus_progress,
                 pbft_consensus_rounds_advanced);
  logger_->debug("Making DAG progress: {} (grew {} dag levels)", making_dag_progress, dag_level_growh);

  logger_->info("Build version: {}", TARAXA_COMMIT_HASH);
  logger_->info("Node addresses: [{}]", node_addresses_);
  logger_->info("Connected to {} peers: [{}]", peers_size, connected_peers_str);
  logger_->debug("Connected peers: [{}]", connected_peers_str_with_ip);
  logger_->info("Number of discovered peers: {}", number_of_discov_peers);
  logger_->debug("Discovered peers: {}", nodes);

  if (is_pbft_syncing) {
    // Syncing...
    const auto percent_synced = (local_pbft_sync_period * 100) / peer_max_pbft_chain_size;
    const auto syncing_time_sec = syncTimeSeconds();
    const auto syncing_peer = pbft_syncing_state_->syncingPeer();

    logger_->info("Syncing for {} seconds, {}% synced", syncing_time_sec, percent_synced);
    logger_->info("Currently syncing from node {}", (syncing_peer ? syncing_peer->getId().abridged() : "None"));
    logger_->info("Max peer PBFT chain size:       {} (peer {})", peer_max_pbft_chain_size, max_pbft_chain_node_id);
    logger_->info("Max peer PBFT consensus round FOR SYNCING:  {} (peer {})", peer_max_pbft_round,
                  max_pbft_round_node_id);
    logger_->info("Max peer DAG level:             {} (peer {})", peer_max_node_dag_level, max_node_dag_level_node_id);
  } else {
    const auto sync_percentage =
        (100 * intervals_in_sync_since_launch_) / (intervals_in_sync_since_launch_ + intervals_syncing_since_launch_);
    logger_->info("In sync since launch for {}% of the time", sync_percentage);
  }
  logger_->info("Max DAG block level in DAG:      {}", local_max_level_in_dag);
  logger_->info("PBFT chain size:                 {} ({})", local_chain_size, local_chain_size_without_empty_blocks);
  logger_->info("Current PBFT period:             {}", local_pbft_period);
  logger_->info("Current PBFT round:              {}", local_pbft_round);
  logger_->info("DPOS total votes count:          {}",
                (local_dpos_total_votes_count.has_value() ? std::to_string(*local_dpos_total_votes_count)
                                                          : "Info not available"));
  logger_->info("PBFT consensus 2t+1 threshold:   {}",
                (local_twotplusone.has_value() ? std::to_string(*local_twotplusone) : "Info not available"));
  logger_->info("Node eligible vote count:        {}", std::to_string(local_dpos_node_votes_count));

  logger_->debug("****** Memory structures sizes ******");
  logger_->debug("Verified votes size:             {}", vote_mgr_->getVerifiedVotesSize());
  logger_->debug("Non finalized txs size:          {}", trx_mgr_->getNonfinalizedTrxSize());
  logger_->debug("Txs pool size:                   {}", trx_mgr_->getTransactionPoolSize());

  const auto [non_finalized_blocks_levels, non_finalized_blocks_size] = dag_mgr_->getNonFinalizedBlocksSize();
  logger_->debug("Non finalized dag blocks levels: {}", non_finalized_blocks_levels);
  logger_->debug("Non finalized dag blocks size:   {}", non_finalized_blocks_size);

  const auto [high_priority_queue_size, mid_priority_queue_size, low_priority_queue_size] =
      thread_pool_->getQueueSize();
  logger_->debug("High priority queue size: {}", high_priority_queue_size);
  logger_->debug("Mid priority queue size: {}", mid_priority_queue_size);
  logger_->debug("Low priority queue size: {}", low_priority_queue_size);

  logger_->info("------------- tl);dr -------------");

  if (making_pbft_chain_progress) {
    if (is_pbft_syncing) {
      logger_->info("STATUS: GOOD. ACTIVELY SYNCING");
    } else if (local_dpos_node_votes_count) {
      logger_->info("STATUS: GOOD. NODE SYNCED AND PARTICIPATING IN CONSENSUS");
    } else {
      logger_->info("STATUS: GOOD. NODE SYNCED");
    }
  } else if (is_pbft_syncing && (making_pbft_sync_period_progress || making_dag_progress)) {
    logger_->info("STATUS: PENDING SYNCED DATA");
  } else if (!is_pbft_syncing && making_pbft_consensus_progress) {
    if (local_dpos_node_votes_count) {
      logger_->info("STATUS: PARTICIPATING IN CONSENSUS BUT NO NEW FINALIZED BLOCKS");
    } else {
      logger_->info("STATUS: NODE SYNCED BUT NO NEW FINALIZED BLOCKS");
    }
  } else if (!is_pbft_syncing && making_dag_progress) {
    logger_->info("STATUS: PBFT STALLED, POSSIBLY PARTITIONED. NODE HAS NOT RESTARTED SYNCING");
  } else if (peers_size) {
    if (is_pbft_syncing) {
      logger_->info("STATUS: SYNCING STALLED. NO PROGRESS MADE IN LAST {} SECONDS", stalled_syncing_duration_seconds);
    } else {
      logger_->info("STATUS: STUCK. NODE HAS NOT RESTARTED SYNCING");
    }
  } else {
    // Peer size is zero...
    logger_->info("STATUS: NOT CONNECTED TO ANY PEERS. POSSIBLE CONFIG ISSUE OR NETWORK CONNECTIVITY");
  }

  logger_->info("In the last {} seconds...", delta);

  if (is_pbft_syncing) {
    logger_->info("PBFT sync period progress:      {}", pbft_sync_period_progress);
  }

  logger_->info("PBFT chain blocks added:        {}", pbft_chain_size_growth);
  logger_->info("PBFT rounds advanced:           {}", pbft_consensus_rounds_advanced);
  logger_->info("DAG level growth:               {}", dag_level_growh);

  logger_->info("##################################");

  // Node stats info history
  local_max_level_in_dag_prev_interval_ = local_max_level_in_dag;
  local_pbft_round_prev_interval_ = local_pbft_round;
  local_chain_size_prev_interval_ = local_chain_size;
  local_pbft_sync_period_prev_interval_ = local_pbft_sync_period;
}

Json::Value NodeStats::getStatus(
    std::map<network::tarcap::TarcapVersion, std::shared_ptr<network::tarcap::TaraxaPeer>> peers) const {
  Json::Value res;
  dev::p2p::NodeID max_pbft_round_nodeID;
  dev::p2p::NodeID max_pbft_chain_nodeID;
  dev::p2p::NodeID max_node_dag_level_nodeID;
  uint64_t peer_max_pbft_round = 1;
  uint64_t peer_max_pbft_chain_size = 1;
  uint64_t peer_max_node_dag_level = 1;

  res["peers"] = Json::Value(Json::arrayValue);

  for (auto const &peer : peers) {
    Json::Value peer_status;
    peer_status["node_id"] = peer.second->getId().toString();
    peer_status["net_version"] = Json::UInt64(peer.first);
    peer_status["dag_level"] = Json::UInt64(peer.second->dag_level_);
    peer_status["pbft_size"] = Json::UInt64(peer.second->pbft_chain_size_);
    peer_status["dag_synced"] = !peer.second->syncing_;
    res["peers"].append(peer_status);
    // Find max pbft chain size
    if (peer.second->pbft_chain_size_ > peer_max_pbft_chain_size) {
      peer_max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_nodeID = peer.second->getId();
    }

    // Find max dag level
    if (peer.second->dag_level_ > peer_max_node_dag_level) {
      peer_max_node_dag_level = peer.second->dag_level_;
      max_node_dag_level_nodeID = peer.second->getId();
    }

    // Find max peer PBFT round
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      max_pbft_round_nodeID = peer.second->getId();
    }
  }

  if (const auto syncing_peer = pbft_syncing_state_->syncingPeer();
      syncing_peer && pbft_syncing_state_->isPbftSyncing()) {
    res["syncing_from_node_id"] = syncing_peer->getId().toString();
  }

  res["peer_max_pbft_round"] = Json::UInt64(peer_max_pbft_round);
  res["peer_max_pbft_chain_size"] = Json::UInt64(peer_max_pbft_chain_size);
  res["peer_max_node_dag_level"] = Json::UInt64(peer_max_node_dag_level);
  res["peer_max_pbft_round_node_id"] = max_pbft_round_nodeID.toString();
  res["peer_max_pbft_chain_size_node_id"] = max_pbft_chain_nodeID.toString();
  res["peer_max_node_dag_level_node_id"] = max_node_dag_level_nodeID.toString();

  return res;
}

}  // namespace taraxa::network::tarcap