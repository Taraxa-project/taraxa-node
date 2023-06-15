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

void DagBlockPacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  // Only one dag block can be received
  if (constexpr size_t required_size = 8; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void DagBlockPacketHandler::process(const threadpool::PacketData &packet_data,
                                    const std::shared_ptr<TaraxaPeer> &peer) {
  DagBlock block(packet_data.rlp_);
  blk_hash_t const hash = block.getHash();

  peer->markDagBlockAsKnown(hash);

  if (block.getLevel() > peer->dag_level_) {
    peer->dag_level_ = block.getLevel();
  }

  // Do not process this block in case we already have it
  if (dag_mgr_->isDagBlockKnown(block.getHash())) {
    LOG(log_tr_) << "Received known DagBlockPacket " << hash << "from: " << peer->getId();
    return;
  }

  onNewBlockReceived(std::move(block), peer);
}

void DagBlockPacketHandler::sendBlock(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block,
                                      const SharedTransactions &trxs) {
  std::shared_ptr<TaraxaPeer> peer = peers_state_->getPeer(peer_id);
  if (!peer) {
    LOG(log_wr_) << "Send dag block " << block.getHash() << ". Failed to obtain peer " << peer_id;
    return;
  }

  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  // Transactions are first sent in transactions packet before sending the block
  uint32_t index = 0;
  while (index < trxs.size()) {
    const uint32_t trx_count_to_send = std::min(static_cast<size_t>(kMaxTransactionsInPacket), trxs.size() - index);

    dev::RLPStream s(TransactionPacketHandler::kTransactionPacketItemCount);
    s.appendList(trx_count_to_send);

    taraxa::bytes trx_bytes;
    for (uint32_t i = index; i < index + trx_count_to_send; i++) {
      auto &trx_data = trxs[i]->rlp();
      s << trxs[i]->getHash();
      trx_bytes.insert(trx_bytes.end(), std::begin(trx_data), std::end(trx_data));
    }

    s.appendList(trx_count_to_send);
    s.appendRaw(trx_bytes, trx_count_to_send);
    sealAndSend(peer_id, TransactionPacket, std::move(s));

    index += trx_count_to_send;
  }

  if (!sealAndSend(peer_id, DagBlockPacket, block.streamRLP(true))) {
    LOG(log_wr_) << "Sending DagBlock " << block.getHash() << " failed to " << peer_id;
    return;
  }

  // Mark data as known if sending was successful
  peer->markDagBlockAsKnown(block.getHash());
  for (const auto &trx : trxs) {
    peer->markTransactionAsKnown(trx->getHash());
  }
}

void DagBlockPacketHandler::onNewBlockReceived(DagBlock &&block, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto block_hash = block.getHash();
  const auto verified = dag_mgr_->verifyBlock(block);
  switch (verified) {
    case DagManager::VerifyBlockReturnType::IncorrectTransactionsEstimation:
    case DagManager::VerifyBlockReturnType::BlockTooBig:
    case DagManager::VerifyBlockReturnType::FailedVdfVerification:
    case DagManager::VerifyBlockReturnType::NotEligible:
    case DagManager::VerifyBlockReturnType::FailedTipsVerification: {
      std::ostringstream err_msg;
      err_msg << "DagBlock " << block_hash << " failed verification with error code "
              << static_cast<uint32_t>(verified);
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
        std::ostringstream err_msg;
        err_msg << "DagBlock has missing tip";
        throw MaliciousPeerException(err_msg.str());
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
      auto transactions = trx_mgr_->getPoolTransactions(block.getTrxs()).first;
      auto status = dag_mgr_->addDagBlock(std::move(block), std::move(transactions));
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
              err_msg << "DagBlock" << block.getHash() << " has missing pivot or/and tips " << status.second;
            else
              err_msg << "DagBlock" << block.getHash() << " could not be added to DAG";
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

void DagBlockPacketHandler::onNewBlockVerified(const DagBlock &block, bool proposed, const SharedTransactions &trxs) {
  // If node is pbft syncing and block is not proposed by us, this is an old block that has been verified - no block
  // gossip is needed
  if (!proposed && pbft_syncing_state_->isDeepPbftSyncing()) {
    return;
  }

  const auto &block_hash = block.getHash();
  LOG(log_tr_) << "Verified NewBlock " << block_hash.toString();

  std::vector<dev::p2p::NodeID> peers_to_send;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isDagBlockKnown(block_hash) && !peer.second->syncing_) {
      peers_to_send.push_back(peer.first);
    }
  }

  // trxs contains only transaction that were not already in some previous DAG block,
  // block_trxs will contain hashes of transactions which were already in DAG
  std::unordered_set<trx_hash_t> block_trxs;
  block_trxs.reserve(block.getTrxs().size());
  for (const auto &trx_hash : block.getTrxs()) {
    block_trxs.insert(trx_hash);
  }
  // Exclude transactions from trxs
  for (const auto &trx : trxs) {
    block_trxs.erase(trx->getHash());
  }

  std::string peer_and_transactions_to_log;
  // Sending it in same order favours some peers over others, always start with a different position
  const auto peers_to_send_count = peers_to_send.size();
  if (peers_to_send_count > 0) {
    uint32_t start_with = rand() % peers_to_send_count;
    for (uint32_t i = 0; i < peers_to_send_count; i++) {
      auto peer_id = peers_to_send[(start_with + i) % peers_to_send_count];
      dev::RLPStream ts;
      auto peer = peers_state_->getPeer(peer_id);
      if (peer && !peer->syncing_) {
        peer_and_transactions_to_log += " Peer: " + peer->getId().abridged() + " Trxs: ";

        SharedTransactions transactions_to_send;
        for (const auto &trx : trxs) {
          const auto &trx_hash = trx->getHash();
          if (peer->isTransactionKnown(trx_hash)) {
            continue;
          }
          transactions_to_send.push_back(trx);
          peer_and_transactions_to_log += trx_hash.abridged();
        }
        for (const auto &trx_hash : block_trxs) {
          if (peer->isTransactionKnown(trx_hash)) {
            continue;
          }
          // Db can be nullptr in some unit tests
          if (db_ != nullptr) {
            auto trx = db_->getTransaction(trx_hash);
            assert(trx != nullptr);
            transactions_to_send.push_back(trx);
            peer_and_transactions_to_log += trx_hash.abridged();
          }
        }

        sendBlock(peer_id, block, transactions_to_send);
        peer->markDagBlockAsKnown(block_hash);
      }
    }
  }
  LOG(log_dg_) << "Send DagBlock " << block.getHash() << " to peers: " << peer_and_transactions_to_log;
  if (!peers_to_send.empty()) LOG(log_tr_) << "Sent block to " << peers_to_send.size() << " peers";
}
}  // namespace taraxa::network::tarcap
