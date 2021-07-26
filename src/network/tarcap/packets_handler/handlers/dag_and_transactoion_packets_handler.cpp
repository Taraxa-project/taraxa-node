#include "dag_and_transactoion_packets_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handler/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagAndTransactionPacketsHandler::DagAndTransactionPacketsHandler(std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<SyncingState> syncing_state,
                                               std::shared_ptr<TransactionManager> trx_mgr,
                                               std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                               std::shared_ptr<DbStorage> db, uint16_t network_min_dag_block_broadcast,
                                               uint16_t network_max_dag_block_broadcast,
                                               uint16_t network_transaction_interval, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), node_addr, "DAG_BLOCK_PH"),
      syncing_state_(syncing_state),
      trx_mgr_(trx_mgr),
      dag_blk_mgr_(dag_blk_mgr),
      db_(db),
      urng_(std::mt19937_64(std::random_device()())),
      network_min_dag_block_broadcast_(network_min_dag_block_broadcast),
      network_max_dag_block_broadcast_(network_max_dag_block_broadcast),
      network_transaction_interval_(network_transaction_interval) {}

void DagAndTransactionPacketsHandler::process(const PacketData &packet_data, const dev::RLP &packet_rlp) {
  if (packet_data.type_ == NewBlockPacket) {
    processNewBlockPacket(packet_data, packet_rlp);
  } else if (packet_data.type_ == NewBlockHashPacket) {
    processNewBlockHashPacket(packet_data, packet_rlp);
  } else if (packet_data.type_ == GetNewBlockPacket) {
    processGetNewBlockPacket(packet_data, packet_rlp);
  } else if (packet_data.type_ == TransactionPacket) {
    processTransactionPacket(packet_data, packet_rlp);
  } else {
    assert(false);
  }
}

inline void DagAndTransactionPacketsHandler::processNewBlockPacket(const PacketData &packet_data, const dev::RLP &packet_rlp) {
  // Ignore new block packets when syncing
  if (syncing_state_->is_syncing()) return;

  DagBlock block(packet_rlp[0].data().toBytes());
  blk_hash_t const hash = block.getHash();

  if (dag_blk_mgr_) {
    if (dag_blk_mgr_->isBlockKnown(hash)) {
      LOG(log_dg_) << "Received NewBlock " << hash.toString() << "that is already known";
      return;
    }
    if (auto status = syncing_state_->checkDagBlockValidation(block); !status.first) {
      LOG(log_wr_) << "Received NewBlock " << hash.toString() << " missing pivot or/and tips";
      status.second.push_back(hash);
      syncing_state_->requestBlocks(packet_data.from_node_id_, status.second,
                                    GetBlocksPacketRequestType::MissingHashes);
      return;
    }
  }

  const auto transactions_count = packet_rlp.itemCount() - 1;
  LOG(log_dg_) << "Received NewBlockPacket " << transactions_count;

  std::vector<Transaction> new_transactions;
  for (size_t i_transaction = 1; i_transaction < transactions_count + 1; i_transaction++) {
    Transaction transaction(packet_rlp[i_transaction].data().toBytes());
    new_transactions.push_back(transaction);
    peer_->markTransactionAsKnown(transaction.getHash());
  }

  peer_->markBlockAsKnown(hash);
  if (block.getLevel() > peer_->dag_level_) peer_->dag_level_ = block.getLevel();
  onNewBlockReceived(block, new_transactions);
}

inline void DagAndTransactionPacketsHandler::processNewBlockHashPacket(const PacketData &packet_data,
                                                              const dev::RLP &packet_rlp) {
  blk_hash_t const hash(packet_rlp[0]);
  LOG(log_dg_) << "Received NewBlockHashPacket " << hash.toString();
  peer_->markBlockAsKnown(hash);

  if (dag_blk_mgr_) {
    if (!dag_blk_mgr_->isBlockKnown(hash) && block_requestes_set_.count(hash) == 0) {
      block_requestes_set_.insert(hash);
      requestBlock(packet_data.from_node_id_, hash);
    }
  } else if (test_blocks_.find(hash) == test_blocks_.end() && block_requestes_set_.count(hash) == 0) {
    block_requestes_set_.insert(hash);
    requestBlock(packet_data.from_node_id_, hash);
  }
}

inline void DagAndTransactionPacketsHandler::processGetNewBlockPacket(const PacketData &packet_data,
                                                             const dev::RLP &packet_rlp) {
  blk_hash_t const hash(packet_rlp[0]);
  peer_->markBlockAsKnown(hash);
  LOG(log_dg_) << "Received GetNewBlockPacket" << hash.toString();

  if (dag_blk_mgr_) {
    auto block = db_->getDagBlock(hash);
    if (block) {
      sendBlock(packet_data.from_node_id_, *block);
    } else
      LOG(log_nf_) << "NO NEW PACKET: " << hash.toString();
  } else if (test_blocks_.find(hash) != test_blocks_.end()) {
    sendBlock(packet_data.from_node_id_, test_blocks_[hash]);
  }
}

inline void DagAndTransactionPacketsHandler::processTransactionPacket(const PacketData & /*packet_data*/,
                                                             const dev::RLP &packet_rlp) {
  std::string received_transactions;
  std::vector<taraxa::bytes> transactions;
  auto transaction_count = packet_rlp.itemCount();
  for (size_t i_transaction = 0; i_transaction < transaction_count; i_transaction++) {
    Transaction transaction(packet_rlp[i_transaction].data().toBytes());
    received_transactions += transaction.getHash().toString() + " ";
    peer_->markTransactionAsKnown(transaction.getHash());
    transactions.emplace_back(packet_rlp[i_transaction].data().toBytes());
  }
  if (transaction_count > 0) {
    LOG(log_dg_) << "Received TransactionPacket with " << packet_rlp.itemCount() << " transactions";
    LOG(log_tr_) << "Received TransactionPacket with " << packet_rlp.itemCount()
                 << " transactions:" << received_transactions.c_str();

    onNewTransactions(transactions, true);
  }
}

void DagAndTransactionPacketsHandler::requestBlock(dev::p2p::NodeID const &peer_id, blk_hash_t hash) {
  LOG(log_dg_) << "requestBlock " << hash.toString();
  peers_state_->sealAndSend(peer_id, GetNewBlockPacket, RLPStream(1) << hash);
}

void DagAndTransactionPacketsHandler::sendBlock(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block) {
  vec_trx_t transactions_to_send;

  if (auto peer = peers_state_->getPeer(peer_id); peer) {
    for (auto trx : block.getTrxs())
      if (!peer->isTransactionKnown(trx)) transactions_to_send.push_back(trx);
  }

  dev::RLPStream s(1 + transactions_to_send.size());
  s.appendRaw(block.rlp(true));

  taraxa::bytes trx_bytes;
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> transaction;
  for (auto trx : transactions_to_send) {
    if (dag_blk_mgr_) {
      transaction = trx_mgr_->getTransaction(trx);
    } else {
      assert(test_transactions_.find(trx) != test_transactions_.end());
      transaction = std::make_shared<std::pair<Transaction, taraxa::bytes>>(test_transactions_[trx],
                                                                            *test_transactions_[trx].rlp());
    }
    assert(transaction != nullptr);  // We should never try to send a block for
                                     // which  we do not have all transactions
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction->second), std::end(transaction->second));
    transaction.reset();
  }
  s.appendRaw(trx_bytes, transactions_to_send.size());
  peers_state_->sealAndSend(peer_id, NewBlockPacket, move(s));
  LOG(log_dg_) << "Send DagBlock " << block.getHash() << " #Trx: " << transactions_to_send.size();
}

void DagAndTransactionPacketsHandler::onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions) {
  LOG(log_nf_) << "Receive DagBlock " << block.getHash() << " #Trx" << transactions.size();
  if (dag_blk_mgr_) {
    LOG(log_nf_) << "Storing block " << block.getHash().toString() << " with " << transactions.size()
                 << " transactions";
    dag_blk_mgr_->insertBroadcastedBlockWithTransactions(block, transactions);

  } else if (test_blocks_.find(block.getHash()) == test_blocks_.end()) {
    test_blocks_[block.getHash()] = block;
    for (auto tr : transactions) {
      test_transactions_[tr.getHash()] = tr;
    }
    onNewBlockVerified(block);

  } else {
    LOG(log_dg_) << "Received NewBlock " << block.getHash().toString() << "that is already known";
    return;
  }
}

void DagAndTransactionPacketsHandler::onNewBlockVerified(DagBlock const &block) {
  // If node is syncing this is an old block that has been verified - no block goosip is needed
  if (syncing_state_->is_syncing()) {
    return;
  }
  LOG(log_dg_) << "Verified NewBlock " << block.getHash().toString();
  auto const peers_without_block =
      selectPeers([&](TaraxaPeer const &_peer) { return !_peer.isBlockKnown(block.getHash()); });

  auto const peers_to_send_number = std::min<std::size_t>(
      std::max<std::size_t>(network_min_dag_block_broadcast_, std::sqrt(peers_state_->getPeersCount())),
      network_max_dag_block_broadcast_);

  std::vector<dev::p2p::NodeID> peers_to_send;
  std::vector<dev::p2p::NodeID> peers_to_announce;
  std::tie(peers_to_send, peers_to_announce) = randomPartitionPeers(peers_without_block, peers_to_send_number);

  for (dev::p2p::NodeID const &peer_id : peers_to_send) {
    RLPStream ts;
    auto peer = peers_state_->getPeer(peer_id);
    if (peer && !peer->syncing_) {
      sendBlock(peer_id, block);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peers_to_send.empty()) LOG(log_dg_) << "Sent block to " << peers_to_send.size() << " peers";

  for (dev::p2p::NodeID const &peer_id : peers_to_announce) {
    RLPStream ts;
    auto peer = peers_state_->getPeer(peer_id);
    if (peer && !peer->syncing_) {
      sendBlockHash(peer_id, block);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peers_to_announce.empty()) LOG(log_dg_) << "Anounced block to " << peers_to_announce.size() << " peers";
}

void DagAndTransactionPacketsHandler::sendBlockHash(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block) {
  LOG(log_dg_) << "sendBlockHash " << block.getHash().toString();
  peers_state_->sealAndSend(peer_id, NewBlockHashPacket, RLPStream(1) << block.getHash());
}

std::pair<std::vector<dev::p2p::NodeID>, std::vector<dev::p2p::NodeID>> DagAndTransactionPacketsHandler::randomPartitionPeers(
    std::vector<dev::p2p::NodeID> const &_peers, std::size_t _number) const {
  vector<dev::p2p::NodeID> part1(_peers);
  vector<dev::p2p::NodeID> part2;

  if (_number >= _peers.size()) return std::make_pair(part1, part2);

  std::shuffle(part1.begin(), part1.end(), urng_);

  // Remove elements from the end of the shuffled part1 vector and move
  // them to part2.
  std::move(part1.begin() + _number, part1.end(), std::back_inserter(part2));
  part1.erase(part1.begin() + _number, part1.end());
  return std::make_pair(move(part1), move(part2));
}

std::vector<dev::p2p::NodeID> DagAndTransactionPacketsHandler::selectPeers(
    std::function<bool(TaraxaPeer const &)> const &_predicate) {
  std::vector<dev::p2p::NodeID> allowed;
  boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
  for (auto const &peer : peers_state_->peers_) {
    if (_predicate(*peer.second)) allowed.push_back(peer.first);
  }
  return allowed;
}

void DagAndTransactionPacketsHandler::onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork) {
  if (fromNetwork) {
    if (dag_blk_mgr_) {
      LOG(log_nf_) << "Storing " << transactions.size() << " transactions";
      received_trx_count_ += transactions.size();
      unique_received_trx_count_ += trx_mgr_->insertBroadcastedTransactions(transactions);
    } else {
      for (auto const &transaction : transactions) {
        Transaction trx(transaction);
        auto trx_hash = trx.getHash();
        if (test_transactions_.find(trx_hash) == test_transactions_.end()) {
          test_transactions_[trx_hash] = trx;
          LOG(log_dg_) << "Received New Transaction " << trx_hash;
        } else {
          LOG(log_dg_) << "Received New Transaction" << trx_hash << "that is already known";
        }
      }
    }
  }
  if (!fromNetwork || network_transaction_interval_ == 0) {
    std::unordered_map<dev::p2p::NodeID, std::vector<taraxa::bytes>> transactions_to_send;
    std::unordered_map<dev::p2p::NodeID, std::vector<trx_hash_t>> transactions_hash_to_send;
    {
      boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
      for (auto &peer : peers_state_->peers_) {
        // Confirm that status messages were exchanged otherwise message might be ignored and node would
        // incorrectly markTransactionAsKnown
        if (!peer.second->syncing_) {
          for (auto const &transaction : transactions) {
            Transaction trx(transaction);
            auto trx_hash = trx.getHash();
            if (!peer.second->isTransactionKnown(trx_hash)) {
              transactions_to_send[peer.first].push_back(transaction);
              transactions_hash_to_send[peer.first].push_back(trx_hash);
            }
          }
        }
      }
    }
    for (auto &it : transactions_to_send) {
      sendTransactions(it.first, it.second);
    }
    boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
    for (auto &it : transactions_hash_to_send) {
      for (auto &it2 : it.second) {
        if (peers_state_->peers_.count(it.first)) peers_state_->peers_[it.first]->markTransactionAsKnown(it2);
      }
    }
  }
}

void DagAndTransactionPacketsHandler::sendTransactions(dev::p2p::NodeID const &peer_id,
                                              std::vector<taraxa::bytes> const &transactions) {
  LOG(log_nf_) << "sendTransactions " << transactions.size() << " to " << peer_id;
  RLPStream s(transactions.size());
  taraxa::bytes trx_bytes;
  for (const auto transaction : transactions) {
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction), std::end(transaction));
  }
  s.appendRaw(trx_bytes, transactions.size());
  peers_state_->sealAndSend(peer_id, TransactionPacket, move(s));
}

}  // namespace taraxa::network::tarcap