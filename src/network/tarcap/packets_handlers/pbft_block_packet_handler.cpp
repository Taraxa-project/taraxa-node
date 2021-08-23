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
                                               std::shared_ptr<DagBlockManager> dag_blk_mgr, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      pbft_chain_(std::move(pbft_chain)),
      dag_blk_mgr_(std::move(dag_blk_mgr)) {}

void PbftBlockPacketHandler::process(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                     const std::shared_ptr<dev::p2p::Host> &host,
                                     const std::shared_ptr<TaraxaPeer> &peer) {
  // Also handle SyncedPacket here
  if (packet_data.type_ == PriorityQueuePacketType::PQ_SyncedPacket) {
    LOG(log_dg_) << "Received synced message from " << packet_data.from_node_id_;
    peer->syncing_ = false;
    return;
  }

  const auto pbft_blk_count = packet_rlp.itemCount();
  LOG(log_dg_) << "PbftBlockPacket received, num of pbft blocks to be processed: " << pbft_blk_count;

  auto period = pbft_chain_->pbftSyncingPeriod();
  for (auto const pbft_blk_tuple : packet_rlp) {
    dev::RLP dag_blocks_rlp = pbft_blk_tuple[0];
    string received_dag_blocks_str;
    std::map<uint64_t, std::map<blk_hash_t, pair<DagBlock, vector<Transaction>>>> dag_blocks_per_level;
    for (auto const dag_blk_struct : dag_blocks_rlp) {
      DagBlock dag_blk(dag_blk_struct[0]);
      auto const &dag_blk_h = dag_blk.getHash();
      peer->markBlockAsKnown(dag_blk_h);
      std::vector<Transaction> new_transactions;
      for (auto const trx_raw : dag_blk_struct[1]) {
        auto &trx = new_transactions.emplace_back(trx_raw);
        peer->markTransactionAsKnown(trx.getHash());
      }
      received_dag_blocks_str += dag_blk_h.toString() + " ";
      auto level = dag_blk.getLevel();
      dag_blocks_per_level[level][dag_blk_h] = {move(dag_blk), move(new_transactions)};
    }
    LOG(log_dg_) << "Received Dag Blocks: " << received_dag_blocks_str;
    for (auto const &block_level : dag_blocks_per_level) {
      for (auto const &block : block_level.second) {
        const auto status = syncing_handler_->checkDagBlockValidation(block.second.first);
        if (!status.first) {
          if (syncing_state_->syncing_peer() == packet_data.from_node_id_) {
            LOG(log_si_) << "PBFT SYNC ERROR, DAG missing a tip/pivot"
                         << ", PBFT chain size: " << pbft_chain_->getPbftChainSize()
                         << ", synced queue size: " << pbft_chain_->pbftSyncedQueueSize();
            syncing_state_->set_peer_malicious();
            host->disconnect(packet_data.from_node_id_, p2p::UserReason);
            syncing_handler_->restartSyncingPbft(true);
          }
          return;
        }
        LOG(log_nf_) << "Storing DAG block " << block.second.first.getHash().toString() << " with "
                     << block.second.second.size() << " transactions";
        if (block.second.first.getLevel() > peer->dag_level_) {
          peer->dag_level_ = block.second.first.getLevel();
        }
        dag_blk_mgr_->processSyncedBlockWithTransactions(block.second.first, block.second.second);
      }
    }

    if (pbft_blk_tuple.itemCount() == 2) {
      PbftBlockCert pbft_blk_and_votes(pbft_blk_tuple[1]);
      auto pbft_blk_hash = pbft_blk_and_votes.pbft_blk->getBlockHash();
      peer->markPbftBlockAsKnown(pbft_blk_hash);
      LOG(log_dg_) << "Processing pbft block: " << pbft_blk_and_votes.pbft_blk->getBlockHash();

      if (pbft_chain_->isKnownPbftBlockForSyncing(pbft_blk_hash)) {
        LOG(log_dg_) << "Block " << pbft_blk_and_votes.pbft_blk->getBlockHash()
                     << " already processed or scheduled to be processed";
        continue;
      }

      if (!pbft_chain_->checkPbftBlockValidationFromSyncing(*pbft_blk_and_votes.pbft_blk)) {
        LOG(log_er_) << "Invalid PBFT block " << pbft_blk_hash << " from peer " << packet_data.from_node_id_.abridged()
                     << " received, stop syncing.";
        syncing_state_->set_peer_malicious();
        host->disconnect(packet_data.from_node_id_, p2p::UserReason);
        syncing_handler_->restartSyncingPbft(true);
        return;
      }

      // Update peer's pbft period if outdated
      if (peer->pbft_chain_size_ < pbft_blk_and_votes.pbft_blk->getPeriod()) {
        peer->pbft_chain_size_ = pbft_blk_and_votes.pbft_blk->getPeriod();
      }

      // Notice: cannot verify 2t+1 cert votes here. Since don't
      // have correct account status for nodes which after the
      // first synced one.
      pbft_chain_->setSyncedPbftBlockIntoQueue(pbft_blk_and_votes);
      period = pbft_chain_->pbftSyncingPeriod();
      LOG(log_nf_) << "Synced PBFT block hash " << pbft_blk_hash << " with " << pbft_blk_and_votes.cert_votes.size()
                   << " cert votes";
      LOG(log_dg_) << "Synced PBFT block " << pbft_blk_and_votes;

      // Reset last sync packet received time
      syncing_state_->set_last_sync_packet_time();
    }
  }
  if (pbft_blk_count > 0) {
    if (syncing_state_->is_pbft_syncing()) {
      // TODO rework
      //   if (period > pbft_chain_->getPbftChainSize() + (10 * conf_.network_sync_level_size)) {
      //     LOG(log_dg_) << "Syncing pbft blocks too fast than processing. Has synced period "
      //                            << period << ", PBFT chain size " << pbft_chain_->getPbftChainSize();
      //     tp_.post(1000, [this, packet_data.from_node_id_] { delayedPbftSync(packet_data.from_node_id_, 1); });
      //   } else {
      syncing_handler_->syncPeerPbft(period + 1);
      //   }
    }
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

void PbftBlockPacketHandler::sendSyncedMessage() {
  LOG(log_dg_) << "sendSyncedMessage ";
  for (const auto &peer : peers_state_->getAllPeersIDs()) {
    sealAndSend(peer, SyncedPacket, RLPStream(0));
  }
}

}  // namespace taraxa::network::tarcap