#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"

#include "network/tarcap/packets/latest/transaction_packet.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

TransactionPacketHandler::TransactionPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                   std::shared_ptr<TransactionManager> trx_mgr, const addr_t &node_addr,
                                                   const std::string &logs_prefix)
    : ITransactionPacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                                logs_prefix + "TRANSACTION_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

inline void TransactionPacketHandler::process(const threadpool::PacketData &packet_data,
                                              const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<TransactionPacket>(packet_data.rlp_);

  if (packet.transactions.size() > kMaxTransactionsInPacket) {
    throw InvalidRlpItemsCountException("TransactionPacket:transactions", packet.transactions.size(),
                                        kMaxTransactionsInPacket);
  }

  if (packet.extra_transactions_hashes.size() > kMaxHashesInPacket) {
    throw InvalidRlpItemsCountException("TransactionPacket:hashes", packet.extra_transactions_hashes.size(),
                                        kMaxHashesInPacket);
  }

  // Extra hashes are hashes of transactions that were not sent as full transactions due to max limit, just mark them as
  // known for sender
  for (const auto &extra_tx_hash : packet.extra_transactions_hashes) {
    peer->markTransactionAsKnown(extra_tx_hash);
  }

  size_t unseen_txs_count = 0;
  size_t data_size = 0;
  for (auto &transaction : packet.transactions) {
    const auto tx_hash = transaction->getHash();
    data_size += transaction->getData().size();
    peer->markTransactionAsKnown(tx_hash);

    // Skip any transactions that are already known to the trx mgr
    if (trx_mgr_->isTransactionKnown(tx_hash)) {
      continue;
    }

    unseen_txs_count++;

    const auto [verified, reason] = trx_mgr_->verifyTransaction(transaction);
    if (!verified) {
      if (reason == "invalid gas" || reason == "gas_price too low") {  // remove after HF
        LOG(log_dg_) << "Transaction " << transaction->getHash() << " has invalid gas or low gas price.";
        trx_mgr_->insertValidatedTransaction(std::move(transaction));
        continue;
      }
      std::ostringstream err_msg;
      err_msg << "DagBlock transaction " << tx_hash << " validation failed: " << reason;
      throw MaliciousPeerException(err_msg.str());
    }

    received_trx_count_++;

    const auto status = trx_mgr_->insertValidatedTransaction(std::move(transaction));
    if (status == TransactionStatus::Inserted) {
      unique_received_trx_count_++;
    }
    if (status == TransactionStatus::Overflow) {
      // Raise exception in trx pool is over the limit and this peer already has too many suspicious packets
      if (peer->reportSuspiciousPacket() && trx_mgr_->nonProposableTransactionsOverTheLimit()) {
        std::ostringstream err_msg;
        err_msg << "Suspicious packets over the limit on DagBlock transaction " << tx_hash << " validation: " << reason;
        throw MaliciousPeerException(err_msg.str());
      }
    }
  }

  // Allow 30% bigger size to support old version, to be removed
  if (data_size > kMaxTransactionsSizeInPacket * 1.3) {
    std::ostringstream err_msg;
    err_msg << "Transactions packet data size over limit " << data_size;
    throw MaliciousPeerException(err_msg.str());
  }

  if (!packet.transactions.empty()) {
    LOG(log_tr_) << "Received TransactionPacket with " << packet.transactions.size() << " transactions";
    LOG(log_dg_) << "Received TransactionPacket with " << packet.transactions.size()
                 << " unseen transactions:" << unseen_txs_count << " from: " << peer->getId().abridged();
  }
}

void TransactionPacketHandler::sendTransactions(std::shared_ptr<TaraxaPeer> peer,
                                                std::pair<SharedTransactions, std::vector<trx_hash_t>> &&transactions) {
  if (!peer) return;
  const auto peer_id = peer->getId();

  LOG(log_tr_) << "sendTransactions " << transactions.first.size() << " to " << peer_id;
  TransactionPacket packet{.transactions = std::move(transactions.first),
                           .extra_transactions_hashes = std::move(transactions.second)};

  if (sealAndSend(peer_id, SubprotocolPacketType::kTransactionPacket, encodePacketRlp(packet))) {
    for (const auto &trx : packet.transactions) {
      peer->markTransactionAsKnown(trx->getHash());
    }
    // Note: do not mark packet.extra_transactions_hashes as known for peer - we are sending just hashes, not full txs
  }
}

}  // namespace taraxa::network::tarcap
