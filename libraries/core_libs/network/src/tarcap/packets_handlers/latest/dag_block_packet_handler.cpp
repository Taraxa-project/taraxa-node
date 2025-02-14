#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"

#include "dag/dag_manager.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagBlockPacketHandler::DagBlockPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                             std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                                             std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                                             const addr_t &node_addr, const std::string &logs_prefix)
    : ExtSyncingPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              logs_prefix + "DAG_BLOCK_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagBlockPacketHandler::process(DagBlockPacket &&packet, const std::shared_ptr<TaraxaPeer> &peer) {
  blk_hash_t const hash = packet.dag_block->getHash();

  for (const auto &tx : packet.transactions) {
    peer->markTransactionAsKnown(tx->getHash());
  }

  peers_state_->markAsKnownForPeerAndConnections(peer, hash,
                                                 [](auto peer, auto hash) { peer->markDagBlockAsKnown(hash); });

  if (packet.dag_block->getLevel() > peer->dag_level_) {
    peer->dag_level_ = packet.dag_block->getLevel();
  }

  // Do not process this block in case we already have it
  if (dag_mgr_->isDagBlockKnown(packet.dag_block->getHash())) {
    LOG(log_tr_) << "Received known DagBlockPacket " << hash << "from: " << peer->getId();
    return;
  }

  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> txs_map;
  txs_map.reserve(packet.transactions.size());
  for (const auto &tx : packet.transactions) {
    txs_map.emplace(tx->getHash(), tx);
  }

  onNewBlockReceived(std::move(packet.dag_block), peer, txs_map);
}

void DagBlockPacketHandler::sendBlockWithTransactions(const std::shared_ptr<TaraxaPeer> &peer,
                                                      const std::shared_ptr<DagBlock> &block,
                                                      SharedTransactions &&trxs) {
  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  DagBlockPacket dag_block_packet{.transactions = std::move(trxs), .dag_block = block};
  if (!sealAndSend(peer->getId(), SubprotocolPacketType::kDagBlockPacket, encodePacketRlp(dag_block_packet))) {
    LOG(log_wr_) << "Sending DagBlock " << block->getHash() << " failed to " << peer->getId();
    return;
  }

  // Mark data as known if sending was successful
  peer->markDagBlockAsKnown(block->getHash());
}

void DagBlockPacketHandler::onNewBlockReceived(
    std::shared_ptr<DagBlock> &&block, const std::shared_ptr<TaraxaPeer> &peer,
    const std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> &trxs) {
  const auto block_hash = block->getHash();
  auto verified = dag_mgr_->verifyBlock(block, trxs);
  switch (verified.first) {
    case DagManager::VerifyBlockReturnType::IncorrectTransactionsEstimation:
    case DagManager::VerifyBlockReturnType::BlockTooBig:
    case DagManager::VerifyBlockReturnType::FailedVdfVerification:
    case DagManager::VerifyBlockReturnType::NotEligible:
    case DagManager::VerifyBlockReturnType::FailedTipsVerification: {
      std::ostringstream err_msg;
      err_msg << "DagBlock " << block_hash << " failed verification with error code "
              << static_cast<uint32_t>(verified.first);
      throw MaliciousPeerException(err_msg.str());
    }
    case DagManager::VerifyBlockReturnType::MissingTransaction:
      if (peer->dagSyncingAllowed()) {
        if (trx_mgr_->transactionsDropped()) [[unlikely]] {
          LOG(log_nf_) << "NewBlock " << block_hash.toString() << " from peer " << peer->getId()
                       << " is missing transaction, our pool recently dropped transactions, requesting dag sync";
        } else {
          LOG(log_wr_) << "NewBlock " << block_hash.toString() << " from peer " << peer->getId()
                       << " is missing transaction, requesting dag sync";
        }
        peer->peer_dag_synced_ = false;
        requestPendingDagBlocks(peer);
      } else {
        if (trx_mgr_->transactionsDropped()) [[unlikely]] {
          // Disconnecting since anything after will also contain missing pivot/tips ...
          LOG(log_nf_) << "NewBlock " << block_hash.toString() << " from peer " << peer->getId()
                       << " is missing transaction, but our pool recently dropped transactions, disconnecting";
          disconnect(peer->getId(), dev::p2p::UserReason);
        } else {
          std::ostringstream err_msg;
          err_msg << "DagBlock" << block_hash << " is missing a transaction while in a dag synced state";
          throw MaliciousPeerException(err_msg.str());
        }
      }
      break;
    case DagManager::VerifyBlockReturnType::MissingTip:
      if (peer->peer_dag_synced_) {
        if (peer->dagSyncingAllowed()) {
          LOG(log_wr_) << "NewBlock " << block_hash.toString() << " from peer " << peer->getId()
                       << " is missing tip, requesting dag sync";
          peer->peer_dag_synced_ = false;
          requestPendingDagBlocks(peer);
        } else {
          std::ostringstream err_msg;
          err_msg << "DagBlock has missing tip";
          throw MaliciousPeerException(err_msg.str());
        }
      } else {
        // peer_dag_synced_ flag ensures that this can only be performed once for a peer
        requestPendingDagBlocks(peer);
      }
      break;
    case DagManager::VerifyBlockReturnType::AheadBlock:
    case DagManager::VerifyBlockReturnType::FutureBlock:
      if (peer->peer_dag_synced_) {
        LOG(log_er_) << "DagBlock" << block_hash << " is an ahead/future block. Peer " << peer->getId()
                     << " will be disconnected";
        disconnect(peer->getId(), dev::p2p::UserReason);
      }
      break;
    case DagManager::VerifyBlockReturnType::Verified: {
      auto status = dag_mgr_->addDagBlock(block, std::move(verified.second));
      if (!status.first) {
        LOG(log_dg_) << "Received DagBlockPacket " << block_hash << "from: " << peer->getId();
        // Ignore new block packets when pbft syncing
        if (pbft_syncing_state_->isPbftSyncing()) {
          LOG(log_dg_) << "Ignore new dag block " << block_hash << ", pbft syncing is on";
        } else if (peer->peer_dag_syncing_) {
          LOG(log_dg_) << "Ignore new dag block " << block_hash << ", dag syncing is on";
        } else {
          if (peer->peer_dag_synced_) {
            std::ostringstream err_msg;
            if (status.second.size() > 0)
              err_msg << "DagBlock" << block->getHash() << " has missing pivot or/and tips " << status.second;
            else
              err_msg << "DagBlock" << block->getHash() << " could not be added to DAG";
            throw MaliciousPeerException(err_msg.str());
          } else {
            // peer_dag_synced_ flag ensures that this can only be performed once for a peer
            requestPendingDagBlocks(peer);
          }
        }
      }
    } break;
    case DagManager::VerifyBlockReturnType::ExpiredBlock:
      break;
  }
}

void DagBlockPacketHandler::onNewBlockVerified(const std::shared_ptr<DagBlock> &block, bool proposed,
                                               const SharedTransactions &trxs) {
  // If node is pbft syncing and block is not proposed by us, this is an old block that has been verified - no block
  // gossip is needed
  if (!proposed && pbft_syncing_state_->isDeepPbftSyncing()) {
    return;
  }

  const auto &block_hash = block->getHash();
  LOG(log_tr_) << "Verified dag block " << block_hash.toString();

  std::vector<dev::p2p::NodeID> peers_to_send;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isDagBlockKnown(block_hash) && !peer.second->syncing_) {
      peers_to_send.push_back(peer.first);
    }
  }

  // Sending it in same order favours some peers over others, always start with a different position
  const auto peers_to_send_count = peers_to_send.size();
  if (peers_to_send_count == 0) {
    return;
  }

  std::string peer_and_transactions_to_log;
  uint32_t start_with = rand() % peers_to_send_count;
  for (uint32_t i = 0; i < peers_to_send_count; i++) {
    auto peer_id = peers_to_send[(start_with + i) % peers_to_send_count];
    auto peer = peers_state_->getPeer(peer_id);
    if (!peer || peer->syncing_) {
      continue;
    }

    peer_and_transactions_to_log += " Peer: " + peer->getId().abridged() + " Trxs: ";

    SharedTransactions transactions_to_send;
    for (const auto &trx : trxs) {
      assert(trx != nullptr);
      const auto trx_hash = trx->getHash();
      if (peer->isTransactionKnown(trx_hash)) {
        continue;
      }

      transactions_to_send.push_back(trx);
      peer_and_transactions_to_log += trx_hash.abridged();
    }

    sendBlockWithTransactions(peer, block, std::move(transactions_to_send));
  }

  LOG(log_dg_) << "Send DagBlock " << block->getHash() << " to peers: " << peer_and_transactions_to_log;
  LOG(log_tr_) << "Sent block to " << peers_to_send.size() << " peers";
}
}  // namespace taraxa::network::tarcap
