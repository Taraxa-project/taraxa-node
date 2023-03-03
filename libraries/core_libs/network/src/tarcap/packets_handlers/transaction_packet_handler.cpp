#include "network/tarcap/packets_handlers/transaction_packet_handler.hpp"

#include <cassert>

#include "network/tarcap/shared_states/test_state.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

TransactionPacketHandler::TransactionPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                   std::shared_ptr<TransactionManager> trx_mgr,
                                                   std::shared_ptr<TestState> test_state, const addr_t &node_addr)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, "TRANSACTION_PH"),
      trx_mgr_(std::move(trx_mgr)),
      test_state_(std::move(test_state)) {}

void TransactionPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kTransactionPacketItemCount) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kTransactionPacketItemCount);
  }
  auto hashes_count = packet_data.rlp_[0].itemCount();
  auto trx_count = packet_data.rlp_[1].itemCount();

  if (hashes_count != trx_count) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, hashes_count, trx_count);
  }
  if (hashes_count == 0 || hashes_count > kMaxTransactionsInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, hashes_count, kMaxTransactionsInPacket);
  }
}

inline void TransactionPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  std::vector<trx_hash_t> received_transactions;

  const auto transaction_count = packet_data.rlp_[0].itemCount();
  received_transactions.reserve(transaction_count);

  std::vector<trx_hash_t> trx_hashes;
  trx_hashes.reserve(transaction_count);

  // First extract only transaction hashes
  for (const auto trx_hash_rlp : packet_data.rlp_[0]) {
    auto trx_hash = trx_hash_rlp.toHash<trx_hash_t>();
    peer->markTransactionAsKnown(trx_hash);
    trx_hashes.emplace_back(std::move(trx_hash));
  }

  for (size_t tx_idx = 0; tx_idx < transaction_count; tx_idx++) {
    const auto &trx_hash = trx_hashes[tx_idx];

    if (trx_mgr_) [[likely]] {  // ONLY FOR TESTING
      // Skip any transactions that are already known to the trx mgr
      if (trx_mgr_->isTransactionKnown(trx_hash)) {
        continue;
      }

      std::shared_ptr<Transaction> transaction;
      // Deserialization is expensive, do it only for the transactions we are about to process
      try {
        transaction = std::make_shared<Transaction>(packet_data.rlp_[1][tx_idx].data().toBytes());
        received_transactions.emplace_back(trx_hash);
      } catch (const Transaction::InvalidTransaction &e) {
        throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
      }

      TransactionStatus status;
      std::string reason;
      std::tie(status, reason) = trx_mgr_->verifyTransaction(transaction);
      switch (status) {
        case TransactionStatus::Invalid: {
          std::ostringstream err_msg;
          err_msg << "DagBlock transaction " << transaction->getHash() << " validation failed: " << reason;
          throw MaliciousPeerException(err_msg.str());
        }
        case TransactionStatus::InsufficentBalance:
        case TransactionStatus::LowNonce: {
          // Raise exception in trx pool is over the limit and this peer already has too many suspicious packets
          if (peer->reportSuspiciousPacket() && trx_mgr_->nonProposableTransactionsOverTheLimit()) {
            std::ostringstream err_msg;
            err_msg << "Suspicious packets over the limit on DagBlock transaction " << transaction->getHash()
                    << " validation: " << reason;
            throw MaliciousPeerException(err_msg.str());
          }

          break;
        }
        case TransactionStatus::Verified:
          break;
        default:
          assert(false);
      }

      received_trx_count_++;
      if (trx_mgr_->insertValidatedTransaction(std::move(transaction), std::move(status))) {
        unique_received_trx_count_++;
      }
    } else {
      // Only for unit tests
      onNewTransactions({std::make_shared<Transaction>(packet_data.rlp_[1][tx_idx].data().toBytes())});
    }
  }

  if (transaction_count > 0) {
    LOG(log_tr_) << "Received TransactionPacket with " << packet_data.rlp_.itemCount() << " transactions";
    LOG(log_dg_) << "Received TransactionPacket with " << received_transactions.size()
                 << " unseen transactions:" << received_transactions << " from: " << peer->getId().abridged();
  }
}

void TransactionPacketHandler::onNewTransactions(const SharedTransactions &transactions) {
  // Only for testing
  for (auto const &trx : transactions) {
    auto trx_hash = trx->getHash();
    if (!test_state_->hasTransaction(trx_hash)) {
      test_state_->insertTransaction(trx);
      LOG(log_tr_) << "Received New Transaction " << trx_hash;
    } else {
      LOG(log_tr_) << "Received New Transaction" << trx_hash << "that is already known";
    }
  }
}

void TransactionPacketHandler::periodicSendTransactions(SharedTransactions &&transactions) {
  std::vector<std::pair<dev::p2p::NodeID, SharedTransactions>> peers_with_transactions_to_send;

  auto peers = peers_state_->getAllPeers();
  std::string transactions_to_log;
  std::string peers_to_log;
  for (auto const &trx : transactions) {
    transactions_to_log += trx->getHash().abridged();
  }
  for (const auto &peer : peers) {
    // Confirm that status messages were exchanged otherwise message might be ignored and node would
    // incorrectly markTransactionAsKnown
    if (!peer.second->syncing_) {
      peers_to_log += peer.first.abridged();
      SharedTransactions peer_trxs;
      for (auto const &trx : transactions) {
        auto trx_hash = trx->getHash();
        if (peer.second->isTransactionKnown(trx_hash)) {
          continue;
        }
        peer_trxs.push_back(trx);
      }
      peers_with_transactions_to_send.push_back({peer.first, peer_trxs});
    }
  }
  const auto peers_to_send_count = peers_with_transactions_to_send.size();
  if (peers_to_send_count > 0) {
    LOG(log_tr_) << "Sending Transactions " << transactions_to_log << " to " << peers_to_log;
    // Sending it in same order favours some peers over others, always start with a different position
    uint32_t start_with = rand() % peers_to_send_count;
    for (uint32_t i = 0; i < peers_to_send_count; i++) {
      auto peer_to_send = peers_with_transactions_to_send[(start_with + i) % peers_to_send_count];
      sendTransactions(peers[peer_to_send.first], std::move(peer_to_send.second));
    }
  }
}

void TransactionPacketHandler::sendTransactions(std::shared_ptr<TaraxaPeer> peer,
                                                std::vector<std::shared_ptr<Transaction>> &&transactions) {
  if (!peer) return;
  const auto peer_id = peer->getId();
  LOG(log_tr_) << "sendTransactions " << transactions.size() << " to " << peer_id;

  uint32_t index = 0;
  while (index < transactions.size()) {
    uint32_t trx_count_to_send = std::min(static_cast<size_t>(kMaxTransactionsInPacket), transactions.size() - index);

    dev::RLPStream s(kTransactionPacketItemCount);
    s.appendList(trx_count_to_send);
    for (uint32_t i = index; i < index + trx_count_to_send; i++) {
      s << transactions[i]->getHash();
    }
    s.appendList(trx_count_to_send);

    for (uint32_t i = index; i < index + trx_count_to_send; i++) {
      const auto &transaction = transactions[i];
      s.appendRaw(transaction->rlp());
    }

    if (sealAndSend(peer_id, TransactionPacket, std::move(s))) {
      for (auto &trx : transactions) {
        peer->markTransactionAsKnown(trx->getHash());
      }
    }

    index += trx_count_to_send;
  }
}

}  // namespace taraxa::network::tarcap
