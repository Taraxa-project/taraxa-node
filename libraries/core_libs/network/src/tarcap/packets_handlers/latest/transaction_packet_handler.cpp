#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"

#include <cassert>

#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

TransactionPacketHandler::TransactionPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                   std::shared_ptr<TransactionManager> trx_mgr, const addr_t &node_addr,
                                                   bool hash_gossip, const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, logs_prefix + "TRANSACTION_PH"),
      trx_mgr_(std::move(trx_mgr)),
      hash_gossip_(hash_gossip) {}

void TransactionPacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kTransactionPacketItemCount) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kTransactionPacketItemCount);
  }
  auto hashes_count = packet_data.rlp_[0].itemCount();
  auto trx_count = packet_data.rlp_[1].itemCount();

  if (hashes_count < trx_count) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, hashes_count, trx_count);
  }
  if (hashes_count == 0 || hashes_count > kMaxTransactionsInPacket + kMaxHashesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, hashes_count,
                                        kMaxTransactionsInPacket + kMaxHashesInPacket);
  }

  if (trx_count > kMaxTransactionsInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, trx_count, kMaxTransactionsInPacket);
  }
}

inline void TransactionPacketHandler::process(const threadpool::PacketData &packet_data,
                                              const std::shared_ptr<TaraxaPeer> &peer) {
  std::vector<trx_hash_t> received_transactions;

  const auto transaction_hashes_count = packet_data.rlp_[0].itemCount();
  const auto transaction_count = packet_data.rlp_[1].itemCount();
  received_transactions.reserve(transaction_count);

  std::vector<trx_hash_t> trx_hashes;
  trx_hashes.reserve(transaction_hashes_count);

  // First extract only transaction hashes
  for (const auto trx_hash_rlp : packet_data.rlp_[0]) {
    auto trx_hash = trx_hash_rlp.toHash<trx_hash_t>();
    peer->markTransactionAsKnown(trx_hash);
    trx_hashes.emplace_back(std::move(trx_hash));
  }

  for (size_t tx_idx = 0; tx_idx < transaction_count; tx_idx++) {
    const auto &trx_hash = trx_hashes[tx_idx];

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
  }

  if (transaction_count > 0) {
    LOG(log_tr_) << "Received TransactionPacket with " << packet_data.rlp_.itemCount() << " transactions";
    LOG(log_dg_) << "Received TransactionPacket with " << received_transactions.size()
                 << " unseen transactions:" << received_transactions << " from: " << peer->getId().abridged();
  }
}

void TransactionPacketHandler::periodicSendTransactionsWithoutHashGossip(
    std::vector<SharedTransactions> &&transactions) {
  std::vector<std::pair<dev::p2p::NodeID, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
      peers_with_transactions_to_send;

  auto peers = peers_state_->getAllPeers();
  for (const auto &peer : peers) {
    // Confirm that status messages were exchanged otherwise message might be ignored and node would
    // incorrectly markTransactionAsKnown
    if (!peer.second->syncing_) {
      SharedTransactions peer_trxs;
      for (auto const &account_trx : transactions) {
        for (auto const &trx : account_trx) {
          auto trx_hash = trx->getHash();
          if (peer.second->isTransactionKnown(trx_hash)) {
            continue;
          }
          peer_trxs.push_back(trx);
        }
      }
      peers_with_transactions_to_send.push_back({peer.first, {peer_trxs, {}}});
    }
  }
  const auto peers_to_send_count = peers_with_transactions_to_send.size();
  if (peers_to_send_count > 0) {
    // Sending it in same order favours some peers over others, always start with a different position
    uint32_t start_with = rand() % peers_to_send_count;
    for (uint32_t i = 0; i < peers_to_send_count; i++) {
      auto peer_to_send = peers_with_transactions_to_send[(start_with + i) % peers_to_send_count];
      sendTransactions(peers[peer_to_send.first], std::move(peer_to_send.second));
    }
  }
}

std::vector<std::pair<std::shared_ptr<TaraxaPeer>, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
TransactionPacketHandler::transactionsToSendToPeers(std::vector<SharedTransactions> &&transactions) {
  // Main goal of the algorithm below is to send different transactions and hashes to different peers but still follow
  // nonce ordering for single account and not send higher nonces without sending low nonces first
  const auto accounts_size = transactions.size();
  std::vector<std::pair<std::shared_ptr<TaraxaPeer>, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
      peers_with_transactions_to_send;
  auto peers = peers_state_->getAllPeers();

  if (accounts_size) {
    // trx_account_index keeps current account start index so that different peers will receive
    // transactions from different accounts
    uint32_t trx_account_index = 0;
    for (const auto &peer : peers) {
      if (!peer.second->syncing_) {
        SharedTransactions peer_trxs;
        std::vector<trx_hash_t> peer_trx_hashes;
        bool trx_max_reached = false;
        uint32_t trx_max_reached_index = 0;
        auto trx_account_index_iterator = trx_account_index;

        while (true) {
          // Iterate over transactions from single account
          for (auto const &trx : transactions[trx_account_index_iterator]) {
            auto trx_hash = trx->getHash();
            if (peer.second->isTransactionKnown(trx_hash)) {
              continue;
            }
            // If max number of transactions to be sent is already reached include hashes to be sent
            if (trx_max_reached) {
              peer_trx_hashes.push_back(trx_hash);
              if (peer_trx_hashes.size() == kMaxHashesInPacket) {
                // If both transactions and hashes reached max nothing to do for this peer, break
                break;
              }
            } else {
              peer_trxs.push_back(trx);
              if (peer_trxs.size() == kMaxTransactionsInPacket) {
                // Max number of transactions reached, save trx_max_reached_index for next peer to continue to avoid
                // sending same transactions to multiple peers
                trx_max_reached = true;
                trx_max_reached_index = (trx_account_index_iterator + 1) % accounts_size;
              }
            }
          }
          if (peer_trx_hashes.size() == kMaxHashesInPacket) {
            // Max transactions and hashes reached, break and move to next peer
            break;
          }
          trx_account_index_iterator = (trx_account_index_iterator + 1) % accounts_size;
          if (trx_account_index_iterator == trx_account_index) {
            // Iterated through all of the transactions, break
            break;
          }
        }
        if (peer_trxs.size() > 0) {
          peers_with_transactions_to_send.push_back({peer.second, {std::move(peer_trxs), std::move(peer_trx_hashes)}});
          if (trx_max_reached) {
            // If transaction max reached, set trx_account_index to start from the next account
            trx_account_index = trx_max_reached_index;
          } else {
            // Since transaction max is not reached, simply continue where the last peer stopped
            trx_account_index = trx_account_index_iterator;
          }
        }
      }
    }
  }
  return peers_with_transactions_to_send;
}

void TransactionPacketHandler::periodicSendTransactions(std::vector<SharedTransactions> &&transactions) {
  // Support of old v2 net version. Remove once network is fully updated
  if (!hash_gossip_) {
    periodicSendTransactionsWithoutHashGossip(std::move(transactions));
    return;
  }

  auto peers_with_transactions_to_send = transactionsToSendToPeers(std::move(transactions));
  const auto peers_to_send_count = peers_with_transactions_to_send.size();
  if (peers_to_send_count > 0) {
    // Sending it in same order favours some peers over others, always start with a different position
    uint32_t start_with = rand() % peers_to_send_count;
    for (uint32_t i = 0; i < peers_to_send_count; i++) {
      auto peer_to_send = peers_with_transactions_to_send[(start_with + i) % peers_to_send_count];
      sendTransactions(peer_to_send.first, std::move(peer_to_send.second));
    }
  }
}

void TransactionPacketHandler::sendTransactions(std::shared_ptr<TaraxaPeer> peer,
                                                std::pair<SharedTransactions, std::vector<trx_hash_t>> &&transactions) {
  if (!peer) return;
  const auto peer_id = peer->getId();
  const auto &transactions_size = transactions.first.size();
  const auto &hashes_size = transactions.second.size();

  LOG(log_tr_) << "sendTransactions " << transactions.first.size() << " to " << peer_id;

  dev::RLPStream s(kTransactionPacketItemCount);
  s.appendList(transactions_size + hashes_size);
  for (const auto &trx : transactions.first) {
    s << trx->getHash();
  }

  for (const auto &trx_hash : transactions.second) {
    s << trx_hash;
  }

  s.appendList(transactions_size);

  for (const auto &trx : transactions.first) {
    s.appendRaw(trx->rlp());
  }

  if (sealAndSend(peer_id, TransactionPacket, std::move(s))) {
    for (const auto &trx : transactions.first) {
      peer->markTransactionAsKnown(trx->getHash());
    }
  }
}

}  // namespace taraxa::network::tarcap
