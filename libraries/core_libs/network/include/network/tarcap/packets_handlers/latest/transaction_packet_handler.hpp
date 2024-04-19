#pragma once

#include "common/packet_handler.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
class TransactionManager;
enum class TransactionStatus;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class TestState;

class TransactionPacketHandler : public PacketHandler {
 public:
  TransactionPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<TransactionManager> trx_mgr, const addr_t& node_addr, bool hash_gossip,
                           const std::string& logs_prefix = "TRANSACTION_PH");

  /**
   * @brief Send transactions
   *
   * @param peer peer to send transactions to
   * @param transactions serialized transactions
   *
   */
  void sendTransactions(std::shared_ptr<TaraxaPeer> peer,
                        std::pair<SharedTransactions, std::vector<trx_hash_t>>&& transactions);

  /**
   * @brief Sends batch of transactions to all connected peers
   * @note This method is used as periodic event to broadcast transactions to the other peers in network
   *
   * @param transactions to be sent
   */
  void periodicSendTransactions(std::vector<SharedTransactions>&& transactions);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::TransactionPacket;

  // 2 items: hashes and transactions
  static constexpr uint32_t kTransactionPacketItemCount = 2;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  /**
   * @brief Sends batch of transactions to all connected peers
   * @note Support of the old V2 version, remove once most of the network is updated or after a hardfork. This method is
   * used as periodic event to broadcast transactions to the other peers in network
   *
   * @param transactions to be sent
   */
  void periodicSendTransactionsWithoutHashGossip(std::vector<SharedTransactions>&& transactions);

  /**
   * @brief select which transactions and hashes to send to which connected peer
   *
   * @param transactions to be sent
   * @return selected transactions and hashes to be sent per peer
   */
  std::vector<std::pair<std::shared_ptr<TaraxaPeer>, std::pair<SharedTransactions, std::vector<trx_hash_t>>>>
  transactionsToSendToPeers(std::vector<SharedTransactions>&& transactions);

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

  std::shared_ptr<TransactionManager> trx_mgr_;

  std::atomic<uint64_t> received_trx_count_{0};
  std::atomic<uint64_t> unique_received_trx_count_{0};
  const bool kHashGossip = true;
};

}  // namespace taraxa::network::tarcap
