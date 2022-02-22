#include "network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "vote/vote.hpp"

namespace taraxa::network::tarcap {

PbftSyncPacketHandler::PbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<PacketsStats> packets_stats,
                                             std::shared_ptr<SyncingState> syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                                             std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                             std::shared_ptr<DbStorage> db, size_t network_sync_level_size,
                                             const addr_t &node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "PBFT_SYNC_PH"),
      network_sync_level_size_(network_sync_level_size),
      delayed_sync_events_tp_(1, true) {}

void PbftSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) {
  checkPacketRlpList(packet_data);

  // TODO: the way we handle PbftSyncPacket must be changed as we are using empty rlp as valid data -> it means pbft
  //       syncing is complete. This is bad - someone could simply send empty PbftSyncPacket over and over again
  if (packet_data.rlp_.itemCount() == 0) {
    return;
  }

  if (size_t required_size = 2; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }

  // SyncBlock rlp parsing cannot be done through util::rlp_tuple, which automatically checks the rlp size so it is
  // checked here manually
  if (size_t required_size = 4; packet_data.rlp_[1].itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_ + ":Syncblock", packet_data.rlp_[1].itemCount(),
                                        required_size);
  }

  // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing
}

void PbftSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  // Note: no need to consider possible race conditions due to concurrent processing as it is
  // disabled on priority_queue blocking dependencies level

  if (syncing_state_->syncing_peer() != packet_data.from_node_id_) {
    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
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
  bool last_block = packet_data.rlp_[0].toInt<bool>();

  SyncBlock sync_block;
  try {
    sync_block = SyncBlock(packet_data.rlp_[1]);
  } catch (const Transaction::InvalidSignature &e) {
    throw MaliciousPeerException("Unable to parse SyncBlock: " + std::string(e.what()));
  }

  const auto pbft_blk_hash = sync_block.pbft_blk->getBlockHash();

  std::string received_dag_blocks_str;  // This is just log related stuff
  for (auto const &block : sync_block.dag_blocks) {
    received_dag_blocks_str += block.getHash().toString() + " ";
    if (peer->dag_level_ < block.getLevel()) {
      peer->dag_level_ = block.getLevel();
    }
  }

  LOG(log_dg_) << "PbftSyncPacket received. Period: " << sync_block.pbft_blk->getPeriod()
               << ", dag Blocks: " << received_dag_blocks_str << " from " << packet_data.from_node_id_;

  peer->markPbftBlockAsKnown(pbft_blk_hash);
  // Update peer's pbft period if outdated
  if (peer->pbft_chain_size_ < sync_block.pbft_blk->getPeriod()) {
    peer->pbft_chain_size_ = sync_block.pbft_blk->getPeriod();
  }

  LOG(log_tr_) << "Processing pbft block: " << pbft_blk_hash;

  if (pbft_chain_->findPbftBlockInChain(pbft_blk_hash)) {
    LOG(log_wr_) << "PBFT block " << pbft_blk_hash << " from " << packet_data.from_node_id_
                 << " already present in chain";
    return;
  }

  if (sync_block.pbft_blk->getPeriod() != pbft_mgr_->pbftSyncingPeriod() + 1) {
    LOG(log_wr_) << "Block " << pbft_blk_hash << " period unexpected: " << sync_block.pbft_blk->getPeriod()
                 << ". Expected period: " << pbft_mgr_->pbftSyncingPeriod() + 1;
    restartSyncingPbft(true);
    return;
  }

  // Check cert vote matches
  for (auto const &vote : sync_block.cert_votes) {
    if (vote->getBlockHash() != pbft_blk_hash) {
      LOG(log_er_) << "Invalid cert votes block hash " << vote->getBlockHash() << " instead of " << pbft_blk_hash
                   << " from peer " << packet_data.from_node_id_.abridged() << " received, stop syncing.";
      handleMaliciousSyncPeer(packet_data.from_node_id_);
      return;
    }
  }

  auto order_hash = PbftManager::calculateOrderHash(sync_block.dag_blocks, sync_block.transactions);
  if (order_hash != sync_block.pbft_blk->getOrderHash()) {
    {  // This is just log related stuff
      std::vector<trx_hash_t> trx_order;
      trx_order.reserve(sync_block.transactions.size());
      std::vector<blk_hash_t> blk_order;
      blk_order.reserve(sync_block.dag_blocks.size());
      for (auto t : sync_block.transactions) {
        trx_order.push_back(t.getHash());
      }
      for (auto b : sync_block.dag_blocks) {
        blk_order.push_back(b.getHash());
      }
      LOG(log_er_) << "Order hash incorrect in sync block " << pbft_blk_hash << " expected: " << order_hash
                   << " received " << sync_block.pbft_blk->getOrderHash() << "; Dag order: " << blk_order
                   << "; Trx order: " << trx_order << "; from " << packet_data.from_node_id_.abridged()
                   << ", stop syncing.";
    }
    handleMaliciousSyncPeer(packet_data.from_node_id_);
    return;
  }

  LOG(log_tr_) << "Synced PBFT block hash " << pbft_blk_hash << " with " << sync_block.cert_votes.size()
               << " cert votes";
  LOG(log_tr_) << "Synced PBFT block " << sync_block;
  pbft_mgr_->syncBlockQueuePush(std::move(sync_block), packet_data.from_node_id_);

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();

  // Reset last sync packet received time
  syncing_state_->set_last_sync_packet_time();

  if (last_block) {
    if (syncing_state_->is_pbft_syncing()) {
      if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * network_sync_level_size_)) {
        LOG(log_tr_) << "Syncing pbft blocks too fast than processing. Has synced period " << pbft_sync_period
                     << ", PBFT chain size " << pbft_chain_->getPbftChainSize();
        delayed_sync_events_tp_.post(1000, [this] { delayedPbftSync(1); });
      } else {
        if (!syncPeerPbft(pbft_sync_period + 1)) {
          return restartSyncingPbft(true);
        }
      }
    }
  }
}

void PbftSyncPacketHandler::pbftSyncComplete() {
  if (pbft_mgr_->syncBlockQueueSize()) {
    LOG(log_tr_) << "Syncing pbft blocks faster than processing. Remaining sync size "
                 << pbft_mgr_->syncBlockQueueSize();
    delayed_sync_events_tp_.post(1000, [this] { pbftSyncComplete(); });
  } else {
    LOG(log_dg_) << "Syncing PBFT is completed";
    // We are pbft synced with the node we are connected to but
    // calling restartSyncingPbft will check if some nodes have
    // greater pbft chain size and we should continue syncing with
    // them, Or sync pending DAG blocks
    restartSyncingPbft(true);
    if (!syncing_state_->is_pbft_syncing()) {
      requestPendingDagBlocks();
    }
  }
}

void PbftSyncPacketHandler::delayedPbftSync(int counter) {
  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (counter > 60) {
    LOG(log_er_) << "Pbft blocks stuck in queue, no new block processed in 60 seconds " << pbft_sync_period << " "
                 << pbft_chain_->getPbftChainSize();
    syncing_state_->set_pbft_syncing(false);
    LOG(log_tr_) << "Syncing PBFT is stopping";
    return;
  }

  if (syncing_state_->is_pbft_syncing()) {
    if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * network_sync_level_size_)) {
      LOG(log_tr_) << "Syncing pbft blocks faster than processing " << pbft_sync_period << " "
                   << pbft_chain_->getPbftChainSize();
      delayed_sync_events_tp_.post(1000, [this, counter] { delayedPbftSync(counter + 1); });
    } else {
      if (!syncPeerPbft(pbft_sync_period + 1)) {
        return restartSyncingPbft(true);
      }
    }
  }
}

void PbftSyncPacketHandler::handleMaliciousSyncPeer(dev::p2p::NodeID const &id) {
  peers_state_->set_peer_malicious(id);

  if (auto host = peers_state_->host_.lock(); host) {
    host->disconnect(id, dev::p2p::UserReason);
  } else {
    LOG(log_er_) << "Unable to handleMaliciousSyncPeer, host == nullptr";
  }
  restartSyncingPbft(true);
}

}  // namespace taraxa::network::tarcap
