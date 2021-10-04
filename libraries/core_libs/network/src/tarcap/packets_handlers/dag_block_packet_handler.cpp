#include "network/tarcap/packets_handlers/dag_block_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/packets_handlers/common/syncing_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagBlockPacketHandler::DagBlockPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
    std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
    std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
    std::shared_ptr<DbStorage> db, std::shared_ptr<TestState> test_state, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "NEW_DAG_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      trx_mgr_(std::move(trx_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)),
      test_state_(std::move(test_state)) {}

thread_local std::mt19937_64 DagBlockPacketHandler::urng_{std::mt19937_64(std::random_device()())};

void DagBlockPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  DagBlock block(packet_data.rlp_[0].data().toBytes());
  blk_hash_t const hash = block.getHash();
  const auto transactions_count = packet_data.rlp_.itemCount() - 1;
  LOG(log_dg_) << "Received DagBlockPacket " << hash.abridged() << " with " << transactions_count << " txs";

  peer->markDagBlockAsKnown(hash);

  std::vector<Transaction> new_transactions;
  for (size_t i_transaction = 1; i_transaction < transactions_count + 1; i_transaction++) {
    Transaction transaction(packet_data.rlp_[i_transaction].data().toBytes());
    peer->markTransactionAsKnown(transaction.getHash());
    new_transactions.push_back(std::move(transaction));
  }

  // Ignore new block packets when pbft syncing
  if (syncing_state_->is_pbft_syncing()) {
    LOG(log_dg_) << "Ignore new dag block " << hash.abridged() << ", pbft syncing is on";
    return;
  }

  if (dag_blk_mgr_) {
    // Do not process this block in case we already have it
    if (dag_blk_mgr_->isDagBlockKnown(block.getHash())) {
      LOG(log_dg_) << "Ignore new dag block " << hash.abridged() << ", block is already known";
      return;
    }

    if (auto status = syncing_handler_->checkDagBlockValidation(block); !status.first) {
      trx_mgr_->insertBroadcastedTransactions(new_transactions);
      LOG(log_wr_) << "Received NewBlock " << hash.toString() << " has missing pivot or/and tips";
      status.second.insert(hash);
      syncing_handler_->requestBlocks(packet_data.from_node_id_, status.second, DagSyncRequestType::MissingHashes);
      return;
    }
  }

  if (block.getLevel() > peer->dag_level_) {
    peer->dag_level_ = block.getLevel();
  }

  onNewBlockReceived(block, new_transactions);
}

void DagBlockPacketHandler::sendBlock(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block) {
  std::shared_ptr<TaraxaPeer> peer = peers_state_->getPeer(peer_id);
  if (!peer) {
    LOG(log_wr_) << "Send dag block " << block.getHash() << ". Failed to obtain peer " << peer_id.abridged();
    return;
  }

  vec_trx_t transactions_to_send;
  for (const auto &trx_hash : block.getTrxs()) {
    if (peer->isTransactionKnown(trx_hash)) {
      continue;
    }
    transactions_to_send.push_back(trx_hash);
  }

  dev::RLPStream s(1 + transactions_to_send.size());
  s.appendRaw(block.rlp(true));

  taraxa::bytes trx_bytes;
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> transaction;
  for (const auto &trx_hash : transactions_to_send) {
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

  // Try to send data over network
  if (!sealAndSend(peer_id, DagBlockPacket, std::move(s))) {
    LOG(log_er_) << "Sending DagBlock " << block.getHash() << " failed";
    return;
  }

  // Mark data as known if sending was successful
  peer->markDagBlockAsKnown(block.getHash());
  for (const auto &trx_hash : transactions_to_send) {
    peer->markTransactionAsKnown(trx_hash);
  }

  LOG(log_dg_) << "Send DagBlock " << block.getHash() << " #Trx: " << transactions_to_send.size();
}

void DagBlockPacketHandler::onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions) {
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

void DagBlockPacketHandler::onNewBlockVerified(DagBlock const &block, bool proposed) {
  // If node is pbft syncing and block is not proposed by us, this is an old block that has been verified - no block
  // goosip is needed
  if (!proposed && syncing_state_->is_pbft_syncing()) {
    return;
  }

  const auto &block_hash = block.getHash();
  LOG(log_dg_) << "Verified NewBlock " << block_hash.toString();

  std::vector<dev::p2p::NodeID> peers_to_send;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isDagBlockKnown(block_hash) && !peer.second->syncing_) {
      peers_to_send.push_back(peer.first);
    }
  }

  for (dev::p2p::NodeID const &peer_id : peers_to_send) {
    dev::RLPStream ts;
    auto peer = peers_state_->getPeer(peer_id);
    if (peer && !peer->syncing_) {
      sendBlock(peer_id, block);
      peer->markDagBlockAsKnown(block_hash);
    }
  }
  if (!peers_to_send.empty()) LOG(log_dg_) << "Sent block to " << peers_to_send.size() << " peers";
}
}  // namespace taraxa::network::tarcap
