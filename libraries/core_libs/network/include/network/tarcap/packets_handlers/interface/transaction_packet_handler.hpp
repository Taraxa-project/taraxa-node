#pragma once

#include "network/tarcap/packets_handlers/latest/common/packet_handler.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::network::tarcap {

class ITransactionPacketHandler : public PacketHandler {
 public:
  ITransactionPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                            std::shared_ptr<TimePeriodPacketsStats> packets_stats, const std::string& logs_prefix);

  /**
   * @brief Sends batch of transactions to all connected peers
   * @note This method is used as periodic event to broadcast transactions to the other peers in network
   *
   * @param transactions to be sent
   */
  void periodicSendTransactions(std::vector<SharedTransactions>&& transactions);

  /**
   * @brief Send transactions
   *
   * @param peer peer to send transactions to
   * @param transactions serialized transactions
   *
   */
  virtual void sendTransactions(std::shared_ptr<TaraxaPeer> peer,
                                std::pair<SharedTransactions, std::vector<trx_hash_t>>&& transactions) = 0;

  /**
   * @brief select which transactions and hashes to send to which connected peer
   *
   * @param transactions to be sent
   * @return selected transactions and hashes to be sent per peer
   */
  std::vector<std::pair<std::shared_ptr<TaraxaPeer>, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
  transactionsToSendToPeers(std::vector<SharedTransactions>&& transactions);

 private:
  /**
   * @brief select which transactions and hashes to send to peer
   *
   * @param peer
   * @param transactions grouped per account to be sent
   * @param account_start_index which account to start with
   * @return index of the next account to continue and selected transactions and hashes to be sent per peer
   */
  std::pair<uint32_t, std::pair<SharedTransactions, std::vector<trx_hash_t>>> transactionsToSendToPeer(
      std::shared_ptr<TaraxaPeer> peer, const std::vector<SharedTransactions>& transactions,
      uint32_t account_start_index);
};

}  // namespace taraxa::network::tarcap
