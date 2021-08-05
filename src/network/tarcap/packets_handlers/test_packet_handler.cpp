#include "test_packet_handler.hpp"

namespace taraxa::network::tarcap {

TestPacketHandler::TestPacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, const addr_t& node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "Test_PH") {}

void TestPacketHandler::process(const PacketData& packet_data, const dev::RLP& packet_rlp) {
  assert(packet_data.type_ == PriorityQueuePacketType::PQ_TestPacket);

  std::scoped_lock lock(mutex_);

  ++cnt_received_messages_[packet_data.from_node_id_];
  test_sums_[packet_data.from_node_id_] += packet_rlp[0].toInt();
}

void TestPacketHandler::sendTestMessage(dev::p2p::NodeID const& _id, int _x, std::vector<char> const& data) {
  sealAndSend(_id, TestPacket, dev::RLPStream(2) << _x << data);
}

std::pair<size_t, uint64_t> TestPacketHandler::retrieveTestData(const dev::p2p::NodeID& node_id) {
  int cnt = 0;
  int checksum = 0;

  std::shared_lock lock(mutex_);

  for (auto i : cnt_received_messages_)
    if (node_id == i.first) {
      cnt += i.second;
      checksum += test_sums_[node_id];
    }

  return {cnt, checksum};
}

}  // namespace taraxa::network::tarcap