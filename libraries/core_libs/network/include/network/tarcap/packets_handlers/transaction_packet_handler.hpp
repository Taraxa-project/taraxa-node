#pragma once

#include "dag/dag_block.hpp"
#include "network/tarcap/packets_handlers/common/packet_handler.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
class TransactionManager;
enum class TransactionStatus;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class TestState;

class TransactionPacketHandler final : public PacketHandler {
 public:
  TransactionPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                           std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<TestState> test_state,
                           const addr_t& node_addr);

  void onNewTransactions(std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>>&& transactions);
  void sendTransactions(dev::p2p::NodeID const& peer_id, std::vector<taraxa::bytes> const& transactions);

  /**
   * @brief Sends batch of transactions to all connected peers
   * @note This method is used as periodic event to broadcast transactions to the other peers in network
   *
   * @param transactions to be sent
   */
  void periodicSendTransactions(SharedTransactions&& transactions);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::TransactionPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<TransactionManager> trx_mgr_;

  // FOR TESTING ONLY
  std::shared_ptr<TestState> test_state_;

  std::atomic<uint64_t> received_trx_count_{0};
  std::atomic<uint64_t> unique_received_trx_count_{0};
};

}  // namespace taraxa::network::tarcap
