#include "pbft_block_packet_handler.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/syncing_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

PbftBlockPacketHandler::PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<PacketsStats> packets_stats,
                                               std::shared_ptr<SyncingState> syncing_state,
                                               std::shared_ptr<SyncingHandler> syncing_handler,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                               size_t network_sync_level_size, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      pbft_chain_(std::move(pbft_chain)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      network_sync_level_size_(network_sync_level_size),
      delayed_sync_events_tp_(1, true) {}

void PbftBlockPacketHandler::process(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                     const std::shared_ptr<dev::p2p::Host> &host,
                                     const std::shared_ptr<TaraxaPeer> &peer) {
  // Also handle SyncedPacket here
  // TODO: create separate handler
  if (packet_data.type_ == PriorityQueuePacketType::PQ_SyncedPacket) {
    LOG(log_dg_) << "Received synced message from " << packet_data.from_node_id_;
    peer->syncing_ = false;
    return;
  }

  if (syncing_state_->syncing_peer() != packet_data.from_node_id_) {
    LOG(log_wr_) << "PbftBlockPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
                 << " current syncing peer " << syncing_state_->syncing_peer().abridged();
    return;
  }

  const size_t item_count = packet_rlp.itemCount();

  // No blocks received - syncing is complete
  if (item_count == 0) {
    pbftSyncComplete();
    return;
  }

  // Process received pbft blocks
  auto it = packet_rlp.begin();
  bool last_block = (*it++).toInt<bool>();
  PbftBlockCert pbft_blk_cert(*it);
  LOG(log_nf_) << "PbftBlockPacket received. Period: " << pbft_blk_cert.pbft_blk->getPeriod();

  string received_dag_blocks_str;
  for (auto const &block_level : pbft_blk_cert.dag_blocks_per_level) {
    for (auto const &block : block_level.second) {
      received_dag_blocks_str += block.getHash().toString() + " ";
    }
  }

  if (!pbft_blk_cert.dag_blocks_per_level.empty()) {
    auto max_level = pbft_blk_cert.dag_blocks_per_level.rbegin()->first;
    if (max_level > peer->dag_level_) {
      peer->dag_level_ = max_level;
    }
  }

  LOG(log_nf_) << "PbftBlockPacket: Received Dag Blocks: " << received_dag_blocks_str;

  auto pbft_blk_hash = pbft_blk_cert.pbft_blk->getBlockHash();
  peer->markPbftBlockAsKnown(pbft_blk_hash);
  LOG(log_dg_) << "Processing pbft block: " << pbft_blk_hash;

  if (pbft_chain_->isKnownPbftBlockForSyncing(pbft_blk_hash)) {
    LOG(log_dg_) << "Block " << pbft_blk_hash << " already processed or scheduled to be processed";
    return;
  }

  if (!pbft_chain_->checkPbftBlockValidationFromSyncing(*pbft_blk_cert.pbft_blk)) {
    LOG(log_er_) << "Invalid PBFT block " << pbft_blk_hash << " from peer " << packet_data.from_node_id_.abridged()
                 << " received, stop syncing.";
    syncing_state_->set_peer_malicious();
    host->disconnect(packet_data.from_node_id_, p2p::UserReason);
    syncing_handler_->restartSyncingPbft(true);
    return;
  }

  // Update peer's pbft period if outdated
  if (peer->pbft_chain_size_ < pbft_blk_cert.pbft_blk->getPeriod()) {
    peer->pbft_chain_size_ = pbft_blk_cert.pbft_blk->getPeriod();
  }

  // Notice: cannot verify 2t+1 cert votes here. Since don't
  // have correct account status for nodes which after the
  // first synced one.
  pbft_chain_->setSyncedPbftBlockIntoQueue(pbft_blk_cert);
  auto pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
  LOG(log_nf_) << "Synced PBFT block hash " << pbft_blk_hash << " with " << pbft_blk_cert.cert_votes.size()
               << " cert votes";
  LOG(log_dg_) << "Synced PBFT block " << pbft_blk_cert;

  // Reset last sync packet received time
  syncing_state_->set_last_sync_packet_time();

  if (last_block) {
    if (syncing_state_->is_pbft_syncing()) {
      if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * network_sync_level_size_)) {
        LOG(log_dg_) << "Syncing pbft blocks too fast than processing. Has synced period " << pbft_sync_period
                     << ", PBFT chain size " << pbft_chain_->getPbftChainSize();
        delayed_sync_events_tp_.post(1000, [this] { delayedPbftSync(1); });
      } else {
        syncing_handler_->syncPeerPbft(pbft_sync_period + 1);
      }
    }
  }
}

void PbftBlockPacketHandler::pbftSyncComplete() {
  if (!pbft_chain_->pbftSyncedQueueEmpty()) {
    LOG(log_dg_) << "Syncing pbft blocks faster than processing. Remaining sync size "
                 << pbft_chain_->pbftSyncedQueueSize();
    delayed_sync_events_tp_.post(1000, [this] { pbftSyncComplete(); });
  } else {
    LOG(log_dg_) << "Syncing PBFT is completed";
    // We are pbft synced with the node we are connected to but
    // calling restartSyncingPbft will check if some nodes have
    // greater pbft chain size and we should continue syncing with
    // them, Or sync pending DAG blocks
    syncing_handler_->restartSyncingPbft(true);
    // We are pbft synced, send message to other node to start
    // gossiping new blocks
    if (!syncing_state_->is_pbft_syncing()) {
      // TODO: Why need to clear all DAG blocks and transactions?
      // This is inside PbftBlockPacket. Why don't clear PBFT blocks and votes?
      sendSyncedMessage();
    }
  }
}

void PbftBlockPacketHandler::delayedPbftSync(int counter) {
  auto pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
  if (counter > 60) {
    LOG(log_er_) << "Pbft blocks stuck in queue, no new block processed in 60 seconds " << pbft_sync_period << " "
                 << pbft_chain_->getPbftChainSize();
    syncing_state_->set_pbft_syncing(false);
    LOG(log_dg_) << "Syncing PBFT is stopping";
    return;
  }

  if (syncing_state_->is_pbft_syncing()) {
    if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * network_sync_level_size_)) {
      LOG(log_dg_) << "Syncing pbft blocks faster than processing " << pbft_sync_period << " "
                   << pbft_chain_->getPbftChainSize();
      delayed_sync_events_tp_.post(1000, [this, counter] { delayedPbftSync(counter + 1); });
    } else {
      syncing_handler_->syncPeerPbft(pbft_sync_period + 1);
    }
  }
}

void PbftBlockPacketHandler::sendSyncedMessage() {
  LOG(log_dg_) << "sendSyncedMessage ";
  for (const auto &peer : peers_state_->getAllPeersIDs()) {
    sealAndSend(peer, SyncedPacket, RLPStream(0));
  }
}

}  // namespace taraxa::network::tarcap