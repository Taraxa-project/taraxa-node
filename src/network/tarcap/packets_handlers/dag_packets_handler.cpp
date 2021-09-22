#include "dag_packets_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/packets_handlers/common/syncing_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagPacketsHandler::DagPacketsHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats,
                                     std::shared_ptr<SyncingState> syncing_state,
                                     std::shared_ptr<SyncingHandler> syncing_handler,
                                     std::shared_ptr<TransactionManager> trx_mgr,
                                     std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                                     std::shared_ptr<TestState> test_state, uint16_t network_min_dag_block_broadcast,
                                     uint16_t network_max_dag_block_broadcast, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "DAG_BLOCKS_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      trx_mgr_(std::move(trx_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)),
      test_state_(std::move(test_state)),
      network_min_dag_block_broadcast_(network_min_dag_block_broadcast),
      network_max_dag_block_broadcast_(network_max_dag_block_broadcast) {}

thread_local mt19937_64 DagPacketsHandler::urng_{std::mt19937_64(std::random_device()())};

void DagPacketsHandler::process(const dev::RLP &packet_rlp, const PacketData &packet_data,

                                const std::shared_ptr<TaraxaPeer> &peer) {
  if (packet_data.type_ == PriorityQueuePacketType::kPqNewBlockPacket) {
    processNewBlockPacket(packet_rlp, packet_data, peer);
  } else if (packet_data.type_ == PriorityQueuePacketType::kPqNewBlockHashPacket) {
    processNewBlockHashPacket(packet_rlp, packet_data, peer);
  } else if (packet_data.type_ == PriorityQueuePacketType::kPqGetNewBlockPacket) {
    processGetNewBlockPacket(packet_rlp, packet_data, peer);
  } else {
    assert(false);
  }
}

inline void DagPacketsHandler::processNewBlockPacket(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                                     const std::shared_ptr<TaraxaPeer> &peer) {
  DagBlock block(packet_rlp[0].data().toBytes());
  blk_hash_t const hash = block.getHash();
  peer->markBlockAsKnown(hash);

  const auto transactions_count = packet_rlp.itemCount() - 1;
  LOG(log_dg_) << "Received NewBlockPacket " << hash.abridged() << " with " << transactions_count << " txs";

  std::vector<Transaction> new_transactions;
  for (size_t i_transaction = 1; i_transaction < transactions_count + 1; i_transaction++) {
    Transaction transaction(packet_rlp[i_transaction].data().toBytes());
    peer->markTransactionAsKnown(transaction.getHash());
    new_transactions.push_back(std::move(transaction));
  }

  // Ignore new block packets when pbft syncing
  if (syncing_state_->is_pbft_syncing()) return;

  if (dag_blk_mgr_) {
    if (auto status = syncing_handler_->checkDagBlockValidation(block); !status.first) {
      LOG(log_wr_) << "Received NewBlock " << hash.toString() << " has missing pivot or/and tips";
      status.second.insert(hash);
      syncing_handler_->requestBlocks(packet_data.from_node_id_, status.second,
                                      GetBlocksPacketRequestType::MissingHashes);
      return;
    }
  }

  if (block.getLevel() > peer->dag_level_) {
    peer->dag_level_ = block.getLevel();
  }

  onNewBlockReceived(block, new_transactions);
}

inline void DagPacketsHandler::processNewBlockHashPacket(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                                         const std::shared_ptr<TaraxaPeer> &peer) {
  blk_hash_t const hash(packet_rlp[0]);
  peer->markBlockAsKnown(hash);

  LOG(log_dg_) << "Received NewBlockHashPacket " << hash.toString();

  if (dag_blk_mgr_) {
    if (!dag_blk_mgr_->isBlockKnown(hash) && insertBlockRequest(hash)) {
      requestBlock(packet_data.from_node_id_, hash);
    }

    return;
  }

  // Functionality needed only in tests...
  // TODO: refactor tests so this is not used anymore
  if (!test_state_->hasBlock(hash) && insertBlockRequest(hash)) {
    requestBlock(packet_data.from_node_id_, hash);
  }
}

inline void DagPacketsHandler::processGetNewBlockPacket(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                                        const std::shared_ptr<TaraxaPeer> &peer) {
  blk_hash_t const hash(packet_rlp[0]);
  LOG(log_dg_) << "Received GetNewBlockPacket" << hash.toString();

  if (dag_blk_mgr_) {
    auto block = dag_blk_mgr_->getDagBlock(hash);
    if (block) {
      sendBlock(packet_data.from_node_id_, *block);
    } else
      LOG(log_nf_) << "Requested block: " << hash.toString() << " not in DB";
  } else if (test_state_->hasBlock(hash)) {
    sendBlock(packet_data.from_node_id_, test_state_->getBlock(hash));
  }
  peer->markBlockAsKnown(hash);
}

void DagPacketsHandler::requestBlock(dev::p2p::NodeID const &peer_id, blk_hash_t hash) {
  LOG(log_dg_) << "requestBlock " << hash.toString();
  sealAndSend(peer_id, GetNewBlockPacket, std::move(dev::RLPStream(1) << hash));
}

void DagPacketsHandler::sendBlock(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block) {
  vec_trx_t transactions_to_send;

  if (auto peer = peers_state_->getPeer(peer_id); peer) {
    for (auto trx_hash : block.getTrxs())
      if (!peer->isTransactionKnown(trx_hash)) transactions_to_send.push_back(trx_hash);
  }

  dev::RLPStream s(1 + transactions_to_send.size());
  s.appendRaw(block.rlp(true));

  taraxa::bytes trx_bytes;
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> transaction;
  for (auto trx_hash : transactions_to_send) {
    if (dag_blk_mgr_) {
      transaction = trx_mgr_->getTransaction(trx_hash);
    } else {
      assert(test_state_->hasTransaction(trx_hash));
      transaction = std::make_shared<std::pair<Transaction, taraxa::bytes>>(
          test_state_->getTransaction(trx_hash), *test_state_->getTransaction(trx_hash).rlp());
    }
    assert(transaction != nullptr);  // We should never try to send a block for
                                     // which  we do not have all transactions
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction->second), std::end(transaction->second));
    transaction.reset();
  }
  s.appendRaw(trx_bytes, transactions_to_send.size());
  sealAndSend(peer_id, NewBlockPacket, move(s));
  LOG(log_dg_) << "Send DagBlock " << block.getHash() << " #Trx: " << transactions_to_send.size();
}

void DagPacketsHandler::onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions) {
  LOG(log_nf_) << "Receive DagBlock " << block.getHash() << " #Trx " << transactions.size();
  if (dag_blk_mgr_) {
    LOG(log_nf_) << "Storing block " << block.getHash().toString() << " with " << transactions.size()
                 << " transactions";
    dag_blk_mgr_->insertBroadcastedBlockWithTransactions(block, transactions);

  } else if (!test_state_->hasBlock(block.getHash())) {
    test_state_->insertBlock(block);
    for (auto tr : transactions) {
      test_state_->insertTransaction(tr);
    }
    onNewBlockVerified(block, false);

  } else {
    LOG(log_dg_) << "Received NewBlock " << block.getHash().toString() << "that is already known";
    return;
  }
}

bool DagPacketsHandler::insertBlockRequest(const blk_hash_t &block_hash) {
  std::unique_lock lock(block_requestes_mutex_);

  return block_requestes_set_.insert(block_hash).second;
}

void DagPacketsHandler::onNewBlockVerified(DagBlock const &block, bool proposed) {
  // If node is pbft syncing and block is not proposed by us, this is an old block that has been verified - no block
  // goosip is needed
  if (!proposed && syncing_state_->is_pbft_syncing()) {
    return;
  }

  const auto &block_hash = block.getHash();
  LOG(log_dg_) << "Verified NewBlock " << block_hash.toString();

  auto const peers_without_block = selectPeers(block_hash);

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
      peer->markBlockAsKnown(block_hash);
    }
  }
  if (!peers_to_send.empty()) LOG(log_dg_) << "Sent block to " << peers_to_send.size() << " peers";

  for (dev::p2p::NodeID const &peer_id : peers_to_announce) {
    RLPStream ts;
    auto peer = peers_state_->getPeer(peer_id);
    if (peer && !peer->syncing_) {
      sendBlockHash(peer_id, block);
      peer->markBlockAsKnown(block_hash);
    }
  }
  if (!peers_to_announce.empty()) LOG(log_dg_) << "Anounced block to " << peers_to_announce.size() << " peers";
}

void DagPacketsHandler::sendBlockHash(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block) {
  LOG(log_dg_) << "sendBlockHash " << block.getHash().toString();
  sealAndSend(peer_id, NewBlockHashPacket, std::move(RLPStream(1) << block.getHash()));
}

std::pair<std::vector<dev::p2p::NodeID>, std::vector<dev::p2p::NodeID>> DagPacketsHandler::randomPartitionPeers(
    std::vector<dev::p2p::NodeID> const &_peers, std::size_t _number) {
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

std::vector<dev::p2p::NodeID> DagPacketsHandler::selectPeers(const blk_hash_t &block_hash) {
  std::vector<dev::p2p::NodeID> allowed;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (peer.second->isBlockKnown(block_hash)) {
      continue;
    }

    allowed.push_back(peer.first);
  }

  return allowed;
}

}  // namespace taraxa::network::tarcap