#include "network/tarcap/packets_handlers/interface/transaction_packet_handler.hpp"

namespace taraxa::network::tarcap {

ITransactionPacketHandler::ITransactionPacketHandler(const FullNodeConfig &conf,
                                                     std::shared_ptr<PeersState> peers_state,
                                                     std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                     const std::string &log_channel_name)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), log_channel_name) {}

void ITransactionPacketHandler::periodicSendTransactions(std::vector<SharedTransactions> &&transactions) {
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

std::vector<std::pair<std::shared_ptr<TaraxaPeer>, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
ITransactionPacketHandler::transactionsToSendToPeers(std::vector<SharedTransactions> &&transactions) {
  // Main goal of the algorithm below is to send different transactions and hashes to different peers but still follow
  // nonce ordering for single account and not send higher nonces without sending low nonces first
  const auto accounts_size = transactions.size();
  if (!accounts_size) {
    return {};
  }
  std::vector<std::pair<std::shared_ptr<TaraxaPeer>, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
      peers_with_transactions_to_send;
  auto peers = peers_state_->getAllPeers();

  // account_index keeps current account index so that different peers will receive
  // transactions from different accounts
  uint32_t account_index = 0;
  for (const auto &peer : peers) {
    if (peer.second->syncing_) {
      continue;
    }

    std::pair<SharedTransactions, std::vector<trx_hash_t>> peer_transactions;
    std::tie(account_index, peer_transactions) = transactionsToSendToPeer(peer.second, transactions, account_index);

    if (peer_transactions.first.size() > 0) {
      peers_with_transactions_to_send.push_back({peer.second, std::move(peer_transactions)});
    }
  }

  return peers_with_transactions_to_send;
}

std::pair<uint32_t, std::pair<SharedTransactions, std::vector<trx_hash_t>>>
ITransactionPacketHandler::transactionsToSendToPeer(std::shared_ptr<TaraxaPeer> peer,
                                                    const std::vector<SharedTransactions> &transactions,
                                                    uint32_t account_start_index) {
  const auto accounts_size = transactions.size();
  bool trx_max_reached = false;
  auto account_iterator = account_start_index;
  std::pair<SharedTransactions, std::vector<trx_hash_t>> result;
  // Next peer should continue after the last account of the current peer
  uint32_t next_peer_account_index = (account_start_index + 1) % accounts_size;

  while (true) {
    // Iterate over transactions from single account
    for (auto const &trx : transactions[account_iterator]) {
      auto trx_hash = trx->getHash();
      if (peer->isTransactionKnown(trx_hash)) {
        continue;
      }
      // If max number of transactions to be sent is already reached include hashes to be sent
      if (trx_max_reached) {
        result.second.push_back(trx_hash);
        if (result.second.size() == kMaxHashesInPacket) {
          // If both transactions and hashes reached max nothing to do for this peer, return
          return {next_peer_account_index, std::move(result)};
        }
      } else {
        result.first.push_back(trx);
        if (result.first.size() == kMaxTransactionsInPacket) {
          // Max number of transactions reached, save next_peer_account_index for next peer to continue to avoid
          // sending same transactions to multiple peers
          trx_max_reached = true;
          next_peer_account_index = (account_iterator + 1) % accounts_size;
        }
      }
    }

    account_iterator = (account_iterator + 1) % accounts_size;
    if (account_iterator == account_start_index) {
      // Iterated through all of the transactions, return
      return {next_peer_account_index, std::move(result)};
    }
  }
}

}  // namespace taraxa::network::tarcap
