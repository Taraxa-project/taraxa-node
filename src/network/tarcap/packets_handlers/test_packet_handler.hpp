#pragma once

#include <libp2p/Common.h>

#include <mutex>

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa::network::tarcap {

class TestPacketHandler : public PacketHandler {
 public:
  TestPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                    const addr_t& node_addr);

  virtual ~TestPacketHandler() = default;

  void sendTestMessage(const dev::p2p::NodeID& id, int x, const std::vector<char>& data);
  std::pair<size_t, uint64_t> retrieveTestData(const dev::p2p::NodeID& node_id);

 private:
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_mutex mutex_;
  std::unordered_map<dev::p2p::NodeID, int> cnt_received_messages_;
  std::unordered_map<dev::p2p::NodeID, int> test_sums_;
};

}  // namespace taraxa::network::tarcap
