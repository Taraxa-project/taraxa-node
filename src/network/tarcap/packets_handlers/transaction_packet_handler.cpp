#include "transaction_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

TransactionPacketHandler::TransactionPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<PacketsStats> packets_stats,
                                                   std::shared_ptr<TransactionManager> trx_mgr,
                                                   std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                                   std::shared_ptr<TestState> test_state,
                                                   uint16_t network_transaction_interval, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "TRANSACTION_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      test_state_(std::move(test_state)),
      network_transaction_interval_(network_transaction_interval) {}

inline void TransactionPacketHandler::process(const dev::RLP &packet_rlp,
                                              const PacketData &packet_data __attribute__((unused)),
                                              const std::shared_ptr<dev::p2p::Host> &host __attribute__((unused)),
                                              const std::shared_ptr<TaraxaPeer> &peer) {
  std::string received_transactions;
  std::vector<taraxa::bytes> transactions;
  auto transaction_count = packet_rlp.itemCount();
  for (size_t i_transaction = 0; i_transaction < transaction_count; i_transaction++) {
    Transaction transaction(packet_rlp[i_transaction].data().toBytes());
    received_transactions += transaction.getHash().abridged() + " ";
    peer->markTransactionAsKnown(transaction.getHash());
    transactions.emplace_back(packet_rlp[i_transaction].data().toBytes());
  }
  if (transaction_count > 0) {
    LOG(log_dg_) << "Received TransactionPacket with " << packet_rlp.itemCount() << " transactions";
    LOG(log_tr_) << "Received TransactionPacket with " << packet_rlp.itemCount()
                 << " transactions:" << received_transactions.c_str();

    onNewTransactions(transactions, true);
  }
}

void TransactionPacketHandler::onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork) {
  if (fromNetwork) {
    if (dag_blk_mgr_) {
      LOG(log_nf_) << "Storing " << transactions.size() << " transactions";
      received_trx_count_ += transactions.size();
      unique_received_trx_count_ += trx_mgr_->insertBroadcastedTransactions(transactions);
    } else {
      for (auto const &transaction : transactions) {
        Transaction trx(transaction);
        auto trx_hash = trx.getHash();
        if (!test_state_->hasTransaction(trx_hash)) {
          test_state_->insertTransaction(trx);
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
    auto peers = peers_state_->getAllPeers();
    for (auto &peer : peers) {
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

    for (auto &it : transactions_to_send) {
      sendTransactions(it.first, it.second);
    }
    for (auto &it : transactions_hash_to_send) {
      for (auto &it2 : it.second) {
        peers[it.first]->markTransactionAsKnown(it2);
      }
    }
  }
}

void TransactionPacketHandler::sendTransactions(dev::p2p::NodeID const &peer_id,
                                                std::vector<taraxa::bytes> const &transactions) {
  LOG(log_nf_) << "sendTransactions " << transactions.size() << " to " << peer_id;

  RLPStream s(transactions.size());
  taraxa::bytes trx_bytes;
  for (const auto &transaction : transactions) {
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction), std::end(transaction));
  }
  s.appendRaw(trx_bytes, transactions.size());
  sealAndSend(peer_id, TransactionPacket, move(s));
}

}  // namespace taraxa::network::tarcap