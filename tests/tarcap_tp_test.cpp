#include <gtest/gtest.h>

#include "dag/dag_block.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packets_handler.hpp"
#include "network/tarcap/threadpool/tarcap_thread_pool.hpp"

namespace taraxa::core_tests {

// Do not use BaseTest from "util_test/gtest.hpp" as its functionality is not needed in this test
struct BaseTest : virtual testing::Test {
  testing::UnitTest* current_test = ::testing::UnitTest::GetInstance();
  testing::TestInfo const* current_test_info = current_test->current_test_info();

  virtual ~BaseTest() {}
};

struct TarcapTpTest : BaseTest {};

using namespace taraxa::network;

class PacketsProcessingInfo {
 public:
  struct PacketProcessingTimes {
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point finish_time_;
  };

 public:
  void addPacketProcessingTimes(tarcap::PacketData::PacketId packet_id,
                                PacketProcessingTimes&& packet_processing_times) {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    bool res = packets_processing_times_.emplace(packet_id, std::move(packet_processing_times)).second;
    assert(res);
  }

  PacketProcessingTimes getPacketProcessingTimes(tarcap::PacketData::PacketId packet_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto found_packet_info = packets_processing_times_.find(packet_id);
    assert(found_packet_info != packets_processing_times_.end());

    return found_packet_info->second;
  }

 private:
  std::unordered_map<tarcap::PacketData::PacketId, PacketProcessingTimes> packets_processing_times_;
  mutable std::shared_mutex mutex_;
};

class DummyPacketHandler : public tarcap::PacketHandler {
 public:
  DummyPacketHandler(std::shared_ptr<tarcap::PeersState> peers_state,
                     std::shared_ptr<tarcap::PacketsStats> packets_stats, const addr_t& node_addr,
                     const std::string& log_channel_name, uint32_t processing_delay_ms,
                     std::shared_ptr<PacketsProcessingInfo> packets_processing_info)
      : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
        processing_delay_ms_(processing_delay_ms),
        packets_processing_info_(std::move(packets_processing_info)) {}

  virtual ~DummyPacketHandler() = default;

 private:
  void process(const tarcap::PacketData& packet_data,
               [[maybe_unused]] const std::shared_ptr<tarcap::TaraxaPeer>& peer) override {
    auto start_time = std::chrono::steady_clock::now();

    LOG(log_dg_) << "Processing packet: " << packet_data.type_str_ << ", id(" << packet_data.id_
                 << ") started. Simulated processing time(sleep): " << processing_delay_ms_ << " [ms]";
    std::this_thread::sleep_for(std::chrono::milliseconds(processing_delay_ms_));
    LOG(log_dg_) << "Processing packet: " << packet_data.type_str_ << ", id(" << packet_data.id_ << ") finished";

    auto finish_time = std::chrono::steady_clock::now();
    packets_processing_info_->addPacketProcessingTimes(packet_data.id_, {start_time, finish_time});
  }

  uint32_t processing_delay_ms_{0};
  std::shared_ptr<PacketsProcessingInfo> packets_processing_info_;
};

TEST_F(TarcapTpTest, dag_block_blocking_deps) {
  const auto own_node_id = dev::p2p::NodeID(1);
  const auto own_node_addr = addr_t(1);

  const auto peers_state = std::make_shared<tarcap::PeersState>(std::weak_ptr<dev::p2p::Host>(), own_node_id);
  const auto packets_stats = std::make_shared<tarcap::PacketsStats>(own_node_addr);
  const auto packets_processing_info = std::make_shared<PacketsProcessingInfo>();

  const auto transaction_packet_handler = std::make_shared<DummyPacketHandler>(
      peers_state, packets_stats, own_node_addr, "TEST_TX_PH", 500, packets_processing_info);
  const auto dag_block_packet_handler = std::make_shared<DummyPacketHandler>(
      peers_state, packets_stats, own_node_addr, "TEST_DAG_BLOCK_PH", 0, packets_processing_info);
  const auto test_packet_handler = std::make_shared<DummyPacketHandler>(peers_state, packets_stats, own_node_addr,
                                                                        "TEST_TEST_PH", 0, packets_processing_info);
  const auto dag_sync_packet_handler = std::make_shared<DummyPacketHandler>(
      peers_state, packets_stats, own_node_addr, "DAG_SYNC_TEST_PH", 1000, packets_processing_info);

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  packets_handler->registerHandler(tarcap::SubprotocolPacketType::TransactionPacket, transaction_packet_handler);
  packets_handler->registerHandler(tarcap::SubprotocolPacketType::DagBlockPacket, dag_block_packet_handler);
  packets_handler->registerHandler(tarcap::SubprotocolPacketType::TestPacket, test_packet_handler);
  packets_handler->registerHandler(tarcap::SubprotocolPacketType::DagSyncPacket, dag_sync_packet_handler);

  dev::p2p::NodeID from_node_id(2);

  // Enable packets from sending peer to be processed
  auto peer = peers_state->addPendingPeer(from_node_id);
  peers_state->setPeerAsReadyToSendMessages(from_node_id, peer);

  // Creates dag block rlp as it is required for blocking mask to extract dag block level
  DagBlock blk(blk_hash_t(10), 1, {}, {}, sig_t(777), blk_hash_t(1), addr_t(15));
  dev::RLPStream s;
  s.appendList(2);
  s.appendRaw(blk.rlp(false));
  s << static_cast<uint8_t>(0);
  std::vector<unsigned char> dag_block_rlp_bytes(s.out());

  // Creates test packets
  tarcap::PacketData tx_packet1(1, tarcap::SubprotocolPacketType::TransactionPacket, dev::p2p::NodeID(from_node_id),
                                {});
  tarcap::PacketData tx_packet2(2, tarcap::SubprotocolPacketType::TransactionPacket, dev::p2p::NodeID(from_node_id),
                                {});
  tarcap::PacketData tx_packet3(3, tarcap::SubprotocolPacketType::TransactionPacket, dev::p2p::NodeID(from_node_id),
                                {});
  tarcap::PacketData dag_sync_packet(4, tarcap::SubprotocolPacketType::DagSyncPacket, dev::p2p::NodeID(from_node_id),
                                     {});
  tarcap::PacketData dag_block_packet(5, tarcap::SubprotocolPacketType::DagBlockPacket, dev::p2p::NodeID(from_node_id),
                                      std::move(dag_block_rlp_bytes));
  tarcap::PacketData test_packet(6, tarcap::SubprotocolPacketType::TestPacket, dev::p2p::NodeID(from_node_id), {});

  // Creates threadpool
  tarcap::TarcapThreadPool tp;
  tp.setPacketsHandlers(packets_handler);

  // Pushes packets to the tp
  tp.push(std::move(tx_packet1));
  tp.push(std::move(tx_packet2));
  tp.push(std::move(dag_sync_packet));

  tp.startProcessing();

  tp.push(std::move(tx_packet3));
  tp.push(std::move(dag_block_packet));
  tp.push(std::move(test_packet));

  // Wait for all packets to be processed
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  // Check order of packets how they were processed
  const auto tx_packet1_processing_info = packets_processing_info->getPacketProcessingTimes(tx_packet1.id_);
  const auto tx_packet2_processing_info = packets_processing_info->getPacketProcessingTimes(tx_packet2.id_);
  const auto tx_packet3_processing_info = packets_processing_info->getPacketProcessingTimes(tx_packet3.id_);
  const auto dag_block_packet_processing_info = packets_processing_info->getPacketProcessingTimes(dag_block_packet.id_);
  const auto test_packet_processing_info = packets_processing_info->getPacketProcessingTimes(test_packet.id_);
  const auto dag_sync_packet_processing_info = packets_processing_info->getPacketProcessingTimes(dag_sync_packet.id_);

  EXPECT_LT(tx_packet1_processing_info.start_time_, tx_packet2_processing_info.start_time_);
  EXPECT_LT(tx_packet2_processing_info.start_time_, dag_sync_packet_processing_info.start_time_);
  EXPECT_LT(dag_sync_packet_processing_info.start_time_, tx_packet3_processing_info.start_time_);
  EXPECT_LT(tx_packet3_processing_info.start_time_, test_packet_processing_info.start_time_);

  EXPECT_LT(test_packet_processing_info.start_time_, tx_packet1_processing_info.finish_time_);
  EXPECT_LT(test_packet_processing_info.start_time_, tx_packet2_processing_info.finish_time_);
  EXPECT_LT(test_packet_processing_info.start_time_, tx_packet3_processing_info.finish_time_);

  EXPECT_GT(dag_block_packet_processing_info.start_time_, tx_packet1_processing_info.finish_time_);
  EXPECT_GT(dag_block_packet_processing_info.start_time_, tx_packet2_processing_info.finish_time_);
  EXPECT_GT(dag_block_packet_processing_info.start_time_, tx_packet3_processing_info.finish_time_);
  EXPECT_GT(dag_block_packet_processing_info.start_time_, dag_sync_packet_processing_info.finish_time_);
}

}  // namespace taraxa::core_tests

int main(int argc, char** argv) {
  using namespace taraxa;

  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}