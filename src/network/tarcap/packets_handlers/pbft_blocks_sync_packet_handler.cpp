#include "pbft_blocks_sync_packet_handler.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/syncing_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

PbftBlocksSyncPacketHandler::PbftBlocksSyncPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
    std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
    std::shared_ptr<DagBlockManager> dag_blk_mgr, size_t network_sync_level_size, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      network_sync_level_size_(network_sync_level_size),
      delayed_sync_events_tp_(1, true) {}

void PbftBlocksSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  // Note: no need to consider possible race conditions due to concurrent processing as it is
  // disabled on priority_queue blocking dependencies level

  // Also handle SyncedPacket here
  // TODO: create separate handler
  if (packet_data.type_ == SubprotocolPacketType::SyncedPacket) {
    LOG(log_dg_) << "Received synced message from " << packet_data.from_node_id_;
    peer->syncing_ = false;
    return;
  }

  if (syncing_state_->syncing_peer() != packet_data.from_node_id_) {
    LOG(log_wr_) << "PbftBlocksSyncPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
                 << " current syncing peer " << syncing_state_->syncing_peer().abridged();
    return;
  }

  const size_t item_count = packet_data.rlp_.itemCount();

  // No blocks received - syncing is complete
  if (item_count == 0) {
    pbftSyncComplete();
    return;
  }

  // Process received pbft blocks
  auto it = packet_data.rlp_.begin();
  bool last_block = (*it++).toInt<bool>();
  SyncBlock sync_block(*it);

  string received_dag_blocks_str;
  for (auto const &block : sync_block.dag_blocks) {
    received_dag_blocks_str += block.getHash().toString() + " ";
    if (peer->dag_level_ < block.getLevel()) {
      peer->dag_level_ = block.getLevel();
    }
  }

  LOG(log_nf_) << "PbftBlocksSyncPacket received. Period: " << sync_block.pbft_blk->getPeriod()
               << ", dag Blocks: " << received_dag_blocks_str;

  auto pbft_blk_hash = sync_block.pbft_blk->getBlockHash();
  peer->markPbftBlockAsKnown(pbft_blk_hash);
  LOG(log_dg_) << "Processing pbft block: " << pbft_blk_hash;

  if (sync_block.pbft_blk->getPeriod() != pbft_mgr_->pbftSyncingPeriod() + 1) {
    LOG(log_dg_) << "Block " << pbft_blk_hash << " period unexpected: " << sync_block.pbft_blk->getPeriod()
                 << ". Expected period: " << pbft_mgr_->pbftSyncingPeriod() + 1;
    return;
  }
  // Update peer's pbft period if outdated
  if (peer->pbft_chain_size_ < sync_block.pbft_blk->getPeriod()) {
    peer->pbft_chain_size_ = sync_block.pbft_blk->getPeriod();
  }

  pbft_mgr_->syncBlockQueuePush(sync_block, packet_data.from_node_id_);

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  LOG(log_nf_) << "Synced PBFT block hash " << pbft_blk_hash << " with " << sync_block.cert_votes.size()
               << " cert votes";
  LOG(log_dg_) << "Synced PBFT block " << sync_block;

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

void PbftBlocksSyncPacketHandler::pbftSyncComplete() {
  if (pbft_mgr_->syncBlockQueueSize()) {
    LOG(log_dg_) << "Syncing pbft blocks faster than processing. Remaining sync size "
                 << pbft_mgr_->syncBlockQueueSize();
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
      // This is inside PbftBlocksSyncPacket. Why don't clear PBFT blocks and votes?
      sendSyncedMessage();
    }
  }
}

void PbftBlocksSyncPacketHandler::delayedPbftSync(int counter) {
  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
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

void PbftBlocksSyncPacketHandler::sendSyncedMessage() {
  LOG(log_dg_) << "sendSyncedMessage ";
  for (const auto &peer : peers_state_->getAllPeersIDs()) {
    sealAndSend(peer, SyncedPacket, RLPStream(0));
  }
}

}  // namespace taraxa::network::tarcap
