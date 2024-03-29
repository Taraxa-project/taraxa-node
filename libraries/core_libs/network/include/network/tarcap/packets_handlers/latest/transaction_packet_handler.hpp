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
                           std::shared_ptr<TransactionManager> trx_mgr, const addr_t& node_addr,
                           const std::string& logs_prefix = "TRANSACTION_PH");

  /**
   * @brief Send transactions
   *
   * @param peer peer to send transactions to
   * @param transactions serialized transactions
   *
   */
  void sendTransactions(std::shared_ptr<TaraxaPeer> peer, std::vector<std::shared_ptr<Transaction>>&& transactions);

  /**
   * @brief Sends batch of transactions to all connected peers
   * @note This method is used as periodic event to broadcast transactions to the other peers in network
   *
   * @param transactions to be sent
   */
  void periodicSendTransactions(SharedTransactions&& transactions);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::TransactionPacket;

  // 2 items: hashes and transactions
  static constexpr uint32_t kTransactionPacketItemCount = 2;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  std::shared_ptr<TransactionManager> trx_mgr_;

  std::atomic<uint64_t> received_trx_count_{0};
  std::atomic<uint64_t> unique_received_trx_count_{0};
};

}  // namespace taraxa::network::tarcap
