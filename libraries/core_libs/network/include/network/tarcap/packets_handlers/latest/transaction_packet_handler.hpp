#pragma once

#include "network/tarcap/packets/latest/transaction_packet.hpp"
#include "network/tarcap/packets_handlers/interface/transaction_packet_handler.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
class TransactionManager;
enum class TransactionStatus;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class TransactionPacketHandler : public ITransactionPacketHandler {
 public:
  TransactionPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<TransactionManager> trx_mgr, const addr_t& node_addr,
                           const std::string& logs_prefix = "");

  /**
   * @brief Send transactions
   *
   * @param peer peer to send transactions to
   * @param transactions serialized transactions
   *
   */
  void sendTransactions(std::shared_ptr<TaraxaPeer> peer,
                        std::pair<SharedTransactions, std::vector<trx_hash_t>>&& transactions) override;

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kTransactionPacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  std::shared_ptr<TransactionManager> trx_mgr_;

  std::atomic<uint64_t> received_trx_count_{0};
  std::atomic<uint64_t> unique_received_trx_count_{0};
};

}  // namespace taraxa::network::tarcap
