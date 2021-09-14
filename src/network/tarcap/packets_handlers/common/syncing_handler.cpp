#include "syncing_handler.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"

namespace taraxa::network::tarcap {

SyncingHandler::SyncingHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                               std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                               std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                               std::shared_ptr<DagBlockManager> dag_blk_mgr, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "SYNCING"),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)) {}

void SyncingHandler::process(const dev::RLP &packet_rlp __attribute__((unused)),
                             const PacketData &packet_data __attribute__((unused)),
                             const std::shared_ptr<dev::p2p::Host> &host __attribute__((unused)),
                             const std::shared_ptr<TaraxaPeer> &peer __attribute__((unused))) {}

void SyncingHandler::restartSyncingPbft(bool force) {
  if (syncing_state_->is_pbft_syncing() && !force) {
    LOG(log_dg_) << "restartSyncingPbft called but syncing_ already true";
    return;
  }

  dev::p2p::NodeID max_pbft_chain_nodeID;
  uint64_t max_pbft_chain_size = 0;
  uint64_t max_node_dag_level = 0;

  if (!peers_state_->getPeersCount()) {
    LOG(log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
    return;
  }
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (peer.second->pbft_chain_size_ > max_pbft_chain_size) {
      max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_nodeID = peer.first;
      max_node_dag_level = peer.second->dag_level_;
    } else if (peer.second->pbft_chain_size_ == max_pbft_chain_size && peer.second->dag_level_ > max_node_dag_level) {
      max_pbft_chain_nodeID = peer.first;
      max_node_dag_level = peer.second->dag_level_;
    }
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (max_pbft_chain_size > pbft_sync_period) {
    LOG(log_si_) << "Restarting syncing PBFT from peer " << max_pbft_chain_nodeID << ", peer PBFT chain size "
                 << max_pbft_chain_size << ", own PBFT chain synced at period " << pbft_sync_period;

    syncing_state_->set_dag_syncing(false);
    syncing_state_->set_pbft_syncing(true, max_pbft_chain_nodeID);
    syncPeerPbft(pbft_sync_period + 1);
  } else {
    LOG(log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                 << pbft_chain_->getPbftChainSize() << ")"
                 << " is greater or equal than max node pbft chain size:" << max_pbft_chain_size;
    syncing_state_->set_pbft_syncing(false);

    if (force || (!syncing_state_->is_dag_syncing() &&
                  max_node_dag_level > std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()))) {
      LOG(log_nf_) << "Request pending " << max_node_dag_level << " "
                   << std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()) << "("
                   << dag_mgr_->getMaxLevel() << ")";
      syncing_state_->set_dag_syncing(true, max_pbft_chain_nodeID);
      requestPendingDagBlocks();
    } else {
      syncing_state_->set_dag_syncing(false);
    }
  }
}

void SyncingHandler::syncPeerPbft(unsigned long height_to_sync) {
  const auto node_id = syncing_state_->syncing_peer();
  LOG(log_nf_) << "Sync peer node " << node_id << " from pbft chain height " << height_to_sync;
  requestPbftBlocks(node_id, height_to_sync);
}

void SyncingHandler::requestPbftBlocks(dev::p2p::NodeID const &_id, size_t height_to_sync) {
  LOG(log_dg_) << "Sending GetPbftBlockPacket with height: " << height_to_sync;
  sealAndSend(_id, SubprotocolPacketType::GetPbftBlockPacket, std::move(dev::RLPStream(1) << height_to_sync));
}

void SyncingHandler::requestPendingDagBlocks() {
  std::unordered_set<blk_hash_t> known_non_finalized_blocks;
  auto blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      known_non_finalized_blocks.insert(block);
    }
  }

  requestBlocks(syncing_state_->syncing_peer(), known_non_finalized_blocks, GetBlocksPacketRequestType::KnownHashes);
}

void SyncingHandler::requestBlocks(const dev::p2p::NodeID &_nodeID, const std::unordered_set<blk_hash_t> &blocks,
                                   GetBlocksPacketRequestType mode) {
  LOG(log_nf_) << "Sending GetBlocksPacket";
  dev::RLPStream s(blocks.size() + 1);  // Mode + block itself
  s << static_cast<uint8_t>(mode);      // Send mode first
  for (const auto &blk : blocks) {
    s << blk;
  }
  sealAndSend(_nodeID, SubprotocolPacketType::GetBlocksPacket, std::move(s));
}

void SyncingHandler::syncPbftNextVotes(uint64_t pbft_round, size_t pbft_previous_round_next_votes_size) {
  dev::p2p::NodeID peer_node_ID;
  uint64_t peer_max_pbft_round = 1;
  size_t peer_max_previous_round_next_votes_size = 0;

  auto peers = peers_state_->getAllPeers();
  // Find max peer PBFT round
  for (auto const &peer : peers) {
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      peer_node_ID = peer.first;
    }
  }

  if (pbft_round == peer_max_pbft_round) {
    // No peers ahead, find peer PBFT previous round max next votes size
    for (auto const &peer : peers) {
      if (peer.second->pbft_previous_round_next_votes_size_ > peer_max_previous_round_next_votes_size) {
        peer_max_previous_round_next_votes_size = peer.second->pbft_previous_round_next_votes_size_;
        peer_node_ID = peer.first;
      }
    }
  }

  if (pbft_round < peer_max_pbft_round ||
      (pbft_round == peer_max_pbft_round &&
       pbft_previous_round_next_votes_size < peer_max_previous_round_next_votes_size)) {
    // TODO: was log_dg_next_votes_sync_
    LOG(log_dg_) << "Syncing PBFT next votes. Current PBFT round " << pbft_round << ", previous round next votes size "
                 << pbft_previous_round_next_votes_size << ". Peer " << peer_node_ID << " is in PBFT round "
                 << peer_max_pbft_round << ", previous round next votes size "
                 << peer_max_previous_round_next_votes_size;
    requestPbftNextVotes(peer_node_ID, pbft_round, pbft_previous_round_next_votes_size);
  }
}

void SyncingHandler::requestPbftNextVotes(dev::p2p::NodeID const &peerID, uint64_t pbft_round,
                                          size_t pbft_previous_round_next_votes_size) {
  // TODO: was log_dg_next_votes_sync_
  LOG(log_dg_) << "Sending GetPbftNextVotes with round " << pbft_round << " previous round next votes size "
               << pbft_previous_round_next_votes_size;
  sealAndSend(peerID, GetPbftNextVotes,
              std::move(dev::RLPStream(2) << pbft_round << pbft_previous_round_next_votes_size));
}

std::pair<bool, std::unordered_set<blk_hash_t>> SyncingHandler::checkDagBlockValidation(const DagBlock &block) const {
  std::unordered_set<blk_hash_t> missing_blks;

  if (dag_blk_mgr_->getDagBlock(block.getHash())) {
    // The DAG block exist
    return std::make_pair(true, missing_blks);
  }

  level_t expected_level = 0;
  for (auto const &tip : block.getTips()) {
    auto tip_block = dag_blk_mgr_->getDagBlock(tip);
    if (!tip_block) {
      LOG(log_er_) << "Block " << block.getHash().toString() << " has a missing tip " << tip.toString();
      missing_blks.insert(tip);
    } else {
      expected_level = std::max(tip_block->getLevel(), expected_level);
    }
  }

  const auto pivot = block.getPivot();
  const auto pivot_block = dag_blk_mgr_->getDagBlock(pivot);
  if (!pivot_block) {
    LOG(log_er_) << "Block " << block.getHash().toString() << " has a missing pivot " << pivot.toString();
    missing_blks.insert(pivot);
  }

  if (missing_blks.size()) return std::make_pair(false, missing_blks);

  expected_level = std::max(pivot_block->getLevel(), expected_level);
  expected_level++;
  if (expected_level != block.getLevel()) {
    LOG(log_er_) << "Invalid block level " << block.getLevel() << " for block " << block.getHash()
                 << " . Expected level " << expected_level;
    return std::make_pair(false, missing_blks);
  }

  return std::make_pair(true, missing_blks);
}

}  // namespace taraxa::network::tarcap