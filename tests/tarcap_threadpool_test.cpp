#include <gtest/gtest.h>

#include <tuple>

#include "config/config.hpp"
#include "dag/dag_block.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packets_handler.hpp"
#include "network/threadpool/tarcap_thread_pool.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

using namespace std::literals;

// Do not use NodesTest from "test_util/gtest.hpp" as its functionality is not needed in this test
struct NodesTest : virtual testing::Test {
  testing::UnitTest* current_test = ::testing::UnitTest::GetInstance();
  testing::TestInfo const* current_test_info = current_test->current_test_info();

  NodesTest() = default;
  virtual ~NodesTest() = default;

  NodesTest(const NodesTest&) = delete;
  NodesTest(NodesTest&&) = delete;
  NodesTest& operator=(const NodesTest&) = delete;
  NodesTest& operator=(NodesTest&&) = delete;
};

struct TarcapTpTest : NodesTest {};

using namespace taraxa::network;

class PacketsProcessingInfo {
 public:
  struct PacketProcessingTimes {
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point finish_time_;
  };

 public:
  void addPacketProcessingTimes(threadpool::PacketData::PacketId packet_id,
                                const PacketProcessingTimes& packet_processing_times) {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    bool res = packets_processing_times_.emplace(packet_id, packet_processing_times).second;
    assert(res);
  }

  PacketProcessingTimes getPacketProcessingTimes(threadpool::PacketData::PacketId packet_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto found_packet_info = packets_processing_times_.find(packet_id);

    // Failed to obtain processing times for packet id: packet_id. Processing did not finish yet. This should be
    // caught in processing times comparing
    if (found_packet_info == packets_processing_times_.end()) {
      return {};
    }

    return found_packet_info->second;
  }

  size_t getPacketProcessingTimesCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return packets_processing_times_.size();
  }

 private:
  std::unordered_map<threadpool::PacketData::PacketId, PacketProcessingTimes> packets_processing_times_;
  mutable std::shared_mutex mutex_;
};

// Help functions for tests
struct HandlersInitData {
  FullNodeConfig conf;
  dev::p2p::NodeID sender_node_id;
  addr_t own_node_addr;

  std::shared_ptr<tarcap::PeersState> peers_state;
  std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats;
  std::shared_ptr<PacketsProcessingInfo> packets_processing_info;

  dev::p2p::NodeID copySender() { return sender_node_id; }
};

class DummyPacketHandler : public tarcap::PacketHandler {
 public:
  DummyPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                     uint32_t processing_delay_ms)
      : PacketHandler(init_data.conf, init_data.peers_state, init_data.packets_stats, init_data.own_node_addr,
                      log_channel_name),
        processing_delay_ms_(processing_delay_ms),
        packets_proc_info_(init_data.packets_processing_info) {}

  virtual ~DummyPacketHandler() = default;
  DummyPacketHandler(const DummyPacketHandler&) = default;
  DummyPacketHandler(DummyPacketHandler&&) = default;
  DummyPacketHandler& operator=(const DummyPacketHandler&) = delete;
  DummyPacketHandler& operator=(DummyPacketHandler&&) = delete;

 private:
  void validatePacketRlpFormat([[maybe_unused]] const threadpool::PacketData& packet_data) const override {}

  void process(const threadpool::PacketData& packet_data,
               [[maybe_unused]] const std::shared_ptr<tarcap::TaraxaPeer>& peer) override {
    // Note do not use LOG() before saving start & finish time as it is internally synchronized and can
    // cause delays, which result in tests fails
    auto start_time = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(processing_delay_ms_));
    auto finish_time = std::chrono::steady_clock::now();

    LOG(log_dg_) << "Processing packet: " << packet_data.type_str_ << ", id(" << packet_data.id_ << ") finished. "
                 << "Start time: " << start_time.time_since_epoch().count()
                 << ", finish time: " << finish_time.time_since_epoch().count();

    packets_proc_info_->addPacketProcessingTimes(packet_data.id_, {start_time, finish_time});
  }

  uint32_t processing_delay_ms_{0};
  std::shared_ptr<PacketsProcessingInfo> packets_proc_info_;
};

class DummyTransactionPacketHandler : public DummyPacketHandler {
 public:
  DummyTransactionPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                                uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::TransactionPacket;
};

class DummyDagBlockPacketHandler : public DummyPacketHandler {
 public:
  DummyDagBlockPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                             uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::DagBlockPacket;
};

class DummyStatusPacketHandler : public DummyPacketHandler {
 public:
  DummyStatusPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                           uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::StatusPacket;
};

class DummyVotePacketHandler : public DummyPacketHandler {
 public:
  DummyVotePacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                         uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::VotePacket;
};

class DummyGetNextVotesBundlePacketHandler : public DummyPacketHandler {
 public:
  DummyGetNextVotesBundlePacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                                       uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetNextVotesSyncPacket;
};

class DummyVotesBundlePacketHandler : public DummyPacketHandler {
 public:
  DummyVotesBundlePacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                                uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::VotesBundlePacket;
};

class DummyGetDagSyncPacketHandler : public DummyPacketHandler {
 public:
  DummyGetDagSyncPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                               uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetDagSyncPacket;
};

class DummyGetPbftSyncPacketHandler : public DummyPacketHandler {
 public:
  DummyGetPbftSyncPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                                uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetPbftSyncPacket;
};

class DummyDagSyncPacketHandler : public DummyPacketHandler {
 public:
  DummyDagSyncPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                            uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::DagSyncPacket;
};

class DummyPbftSyncPacketHandler : public DummyPacketHandler {
 public:
  DummyPbftSyncPacketHandler(const HandlersInitData& init_data, const std::string& log_channel_name,
                             uint32_t processing_delay_ms)
      : DummyPacketHandler(init_data, log_channel_name, processing_delay_ms) {}

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PbftSyncPacket;
};

HandlersInitData createHandlersInitData() {
  HandlersInitData ret_init_data;

  ret_init_data.sender_node_id = dev::p2p::NodeID(1);
  ret_init_data.own_node_addr = addr_t(2);
  ret_init_data.peers_state = std::make_shared<tarcap::PeersState>(std::weak_ptr<dev::p2p::Host>(), FullNodeConfig());
  ret_init_data.packets_stats =
      std::make_shared<tarcap::TimePeriodPacketsStats>(std::chrono::milliseconds(0), ret_init_data.own_node_addr);
  ret_init_data.packets_processing_info = std::make_shared<PacketsProcessingInfo>();

  // Enable packets from sending peer to be processed
  auto peer = ret_init_data.peers_state->addPendingPeer(ret_init_data.sender_node_id);
  ret_init_data.peers_state->setPeerAsReadyToSendMessages(ret_init_data.sender_node_id, peer);

  return ret_init_data;
}

std::pair<tarcap::TarcapVersion, threadpool::PacketData> createPacket(
    const dev::p2p::NodeID& sender_node_id, SubprotocolPacketType packet_type,
    std::optional<std::vector<unsigned char>> packet_rlp_bytes = {}) {
  if (packet_rlp_bytes.has_value()) {
    threadpool::PacketData packet_data(packet_type, sender_node_id, std::move(packet_rlp_bytes.value()));
    return {TARAXA_NET_VERSION, std::move(packet_data)};
  }

  dev::RLPStream s(0);
  threadpool::PacketData packet_data(packet_type, sender_node_id, s.invalidate());
  return {TARAXA_NET_VERSION, std::move(packet_data)};
}

bytes createDagBlockRlp(level_t level, uint32_t sig = 777) {
  // Creates dag block rlp as it is required for blocking mask to extract dag block level
  DagBlock blk(blk_hash_t(10), level, {}, {}, sig_t(sig), blk_hash_t(1), addr_t(15));
  return blk.rlp(true);
}

/**
 * @brief Check all combinations(without repetition) of provided packets that they were processed concurrently:
 *          - packet1.start_time < packet2.finish_time
 *          - packet2.start_time < packet1.finish_time
 *
 * @param packets
 */
void checkConcurrentProcessing(
    const std::vector<std::pair<PacketsProcessingInfo::PacketProcessingTimes, std::string>>& packets) {
  assert(packets.size() >= 2);

  for (size_t i = 0; i < packets.size(); i++) {
    const auto& packet_l = packets[0];
    for (size_t j = i + 1; j < packets.size(); j++) {
      const auto& packet_r = packets[j];
      EXPECT_LT(packet_l.first.start_time_, packet_r.first.finish_time_)
          << packet_l.second << ".start_time < " << packet_r.second << ".finish_time";
      EXPECT_LT(packet_r.first.start_time_, packet_l.first.finish_time_)
          << packet_r.second << ".start_time < " << packet_l.second << ".finish_time";
    }
  }
}

/**
 * @brief Check all combinations(without repetition) of provided packets that they were processed serial:
 *          - packet1.finish_time < packet2.start_time
 *
 * @param packets
 */
void checkSerialProcessing(
    const std::vector<std::pair<PacketsProcessingInfo::PacketProcessingTimes, std::string>>& packets) {
  assert(packets.size() >= 2);

  for (size_t i = 0; i < packets.size(); i++) {
    const auto& packet_l = packets[0];
    for (size_t j = i + 1; j < packets.size(); j++) {
      const auto& packet_r = packets[j];
      EXPECT_LT(packet_l.first.finish_time_, packet_r.first.start_time_)
          << packet_l.second << ".finish_time < " << packet_r.second << ".start_time";
    }
  }
}

size_t queuesSize(const threadpool::PacketsThreadPool& tp) {
  const auto [high_priority_queue_size, mid_priority_queue_size, low_priority_queue_size] = tp.getQueueSize();

  return high_priority_queue_size + mid_priority_queue_size + low_priority_queue_size;
}

// Threshold for packets queue to be emptied
constexpr std::chrono::milliseconds QUEUE_EMPTIED_WAIT_TRESHOLD_MS = 15ms;

// Test if all "block-free" packets are processed concurrently
// Note: in case someone creates new blocking dependency and does not adjust tests, this test should fail
TEST_F(TarcapTpTest, block_free_packets) {
  HandlersInitData init_data = createHandlersInitData();

  // Creates sender 2 to bypass peer order block on Transaction -> DagBlock packet. In case those packets sent
  // 2 different senders those packets are "block-free"
  dev::p2p::NodeID sender2(3);
  auto peer = init_data.peers_state->addPendingPeer(sender2);
  init_data.peers_state->setPeerAsReadyToSendMessages(sender2, peer);

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();

  packets_handler->registerHandler<DummyTransactionPacketHandler>(init_data, "TX_PH", 20);
  packets_handler->registerHandler<DummyDagBlockPacketHandler>(init_data, "DAG_BLOCK_PH", 20);
  packets_handler->registerHandler<DummyStatusPacketHandler>(init_data, "STATUS_PH", 20);
  packets_handler->registerHandler<DummyVotePacketHandler>(init_data, "VOTE_PH", 20);
  packets_handler->registerHandler<DummyGetNextVotesBundlePacketHandler>(init_data, "GET_NEXT_VOTES_SYNC_PH", 20);
  packets_handler->registerHandler<DummyVotesBundlePacketHandler>(init_data, "VOTES_SYNC_PH", 20);

  // Creates threadpool
  // Note: make num of threads >= num of packets to check if they are processed concurrently without blocks, otherwise
  //       some blocks would be blocked for processing due to max threads limit
  threadpool::PacketsThreadPool tp(18);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  // Pushes packets to the tp
  auto packet = createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket, {});
  if (packet.second.rlp_.isList()) {
    std::cout << "is list";
  } else {
    std::cout << "not list";
  }
  const auto packet0_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket, {})).value();
  const auto packet1_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket, {})).value();
  const auto packet2_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket, {})).value();
  const auto packet3_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket, {})).value();

  const auto packet4_dag_block_id =
      tp.push(createPacket(dev::p2p::NodeID(sender2), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(0, 1)}))
          .value();
  const auto packet5_dag_block_id =
      tp.push(createPacket(dev::p2p::NodeID(sender2), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(0, 2)}))
          .value();

  const auto packet8_status_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::StatusPacket, {})).value();
  const auto packet9_status_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::StatusPacket, {})).value();

  const auto packet12_vote_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::VotePacket, {})).value();
  const auto packet13_vote_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::VotePacket, {})).value();

  const auto packet14_get_pbft_next_votes_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetNextVotesSyncPacket, {})).value();
  const auto packet15_get_pbft_next_votes_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetNextVotesSyncPacket, {})).value();

  const auto packet16_pbft_next_votes_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::VotesBundlePacket, {})).value();

  size_t packets_count = 0;
  const auto packet17_pbft_next_votes_id = packets_count =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::VotesBundlePacket, {})).value();

  tp.startProcessing();

  // How should packets be processed:
  // Note: To understand how are different packet types processed (concurrently without any blocking dependencies or
  // synchronously due to some blocking dependencies - depends on situation), check
  // PriorityQueue::updateDependenciesStart
  /*
    ----------------------
    - packet0_transaction -
    ----------------------
    ----------------------
    - packet1_transaction -
    ----------------------
    -----------------------
    - packet2_transaction -
    -----------------------

             -||-
             ...

    -----------------------
    - packet17_votes_sync -
    -----------------------
    0.....................20.................... time [ms]
  */

  // All packets should be already being processed after short amount of time
  std::this_thread::sleep_for(QUEUE_EMPTIED_WAIT_TRESHOLD_MS);
  EXPECT_EQ(queuesSize(tp), 0);

  // Wait until processing of all packets is finished - in some edge cases it might be little bit delayed due to locking
  EXPECT_HAPPENS({500s, 20ms}, [&](auto& ctx) {
    // Check if transactions was propagated to node0
    WAIT_EXPECT_EQ(ctx, init_data.packets_processing_info->getPacketProcessingTimesCount(), packets_count + 1)
  });

  // Check order of packets how they were processed
  const auto packets_proc_info = init_data.packets_processing_info;

  const auto packet0_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet0_tx_id);
  const auto packet1_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet1_tx_id);
  const auto packet2_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet2_tx_id);
  const auto packet3_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet3_tx_id);

  const auto packet4_dag_block_proc_info = packets_proc_info->getPacketProcessingTimes(packet4_dag_block_id);
  const auto packet5_dag_block_proc_info = packets_proc_info->getPacketProcessingTimes(packet5_dag_block_id);

  const auto packet8_status_proc_info = packets_proc_info->getPacketProcessingTimes(packet8_status_id);
  const auto packet9_status_proc_info = packets_proc_info->getPacketProcessingTimes(packet9_status_id);

  const auto packet12_vote_proc_info = packets_proc_info->getPacketProcessingTimes(packet12_vote_id);
  const auto packet13_vote_proc_info = packets_proc_info->getPacketProcessingTimes(packet13_vote_id);

  const auto packet14_get_pbft_next_votes_proc_info =
      packets_proc_info->getPacketProcessingTimes(packet14_get_pbft_next_votes_id);
  const auto packet15_get_pbft_next_votes_proc_info =
      packets_proc_info->getPacketProcessingTimes(packet15_get_pbft_next_votes_id);

  const auto packet16_pbft_next_votes_proc_info =
      packets_proc_info->getPacketProcessingTimes(packet16_pbft_next_votes_id);
  const auto packet17_pbft_next_votes_proc_info =
      packets_proc_info->getPacketProcessingTimes(packet17_pbft_next_votes_id);

  checkConcurrentProcessing({
      {packet0_tx_proc_info, "packet0_tx"},
      {packet1_tx_proc_info, "packet1_tx"},
      {packet2_tx_proc_info, "packet2_tx"},
      {packet3_tx_proc_info, "packet3_tx"},
      {packet4_dag_block_proc_info, "packet4_dag_block"},
      {packet5_dag_block_proc_info, "packet5_dag_block"},
      {packet8_status_proc_info, "packet8_status"},
      {packet9_status_proc_info, "packet9_status"},
      {packet12_vote_proc_info, "packet12_vote"},
      {packet13_vote_proc_info, "packet13_vote"},
      {packet14_get_pbft_next_votes_proc_info, "packet14_get_pbft_next_votes"},
      {packet15_get_pbft_next_votes_proc_info, "packet15_get_pbft_next_votes"},
      {packet16_pbft_next_votes_proc_info, "packet16_pbft_next_votes"},
      {packet17_pbft_next_votes_proc_info, "packet17_pbft_next_votes"},
  });
}

// Test "hard blocking dependencies" related synchronous processing of certain packets:
//
// Packets types that are currently hard blocked for processing in another threads due to dependencies,
// e.g. syncing packets must be processed synchronously one by one, etc...
// Each packet type might be simultaneously blocked by multiple different packets that are being processed.
TEST_F(TarcapTpTest, hard_blocking_deps) {
  HandlersInitData init_data = createHandlersInitData();

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  packets_handler->registerHandler<DummyGetDagSyncPacketHandler>(init_data, "GET_DAG_SYNC_PH", 20);
  packets_handler->registerHandler<DummyGetPbftSyncPacketHandler>(init_data, "GET_PBFT_SYNC_PH", 20);
  packets_handler->registerHandler<DummyDagSyncPacketHandler>(init_data, "DAG_SYNC_PH", 20);
  packets_handler->registerHandler<DummyPbftSyncPacketHandler>(init_data, "PBFT_SYNC_PH", 20);

  // Creates threadpool
  threadpool::PacketsThreadPool tp(10);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  // Pushes packets to the tp
  const auto packet0_dag_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagSyncPacket, {})).value();
  const auto packet1_dag_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagSyncPacket, {})).value();
  const auto packet2_get_dag_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetDagSyncPacket, {})).value();
  const auto packet3_get_dag_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetDagSyncPacket, {})).value();
  const auto packet4_get_pbft_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetPbftSyncPacket, {})).value();
  const auto packet5_get_pbft_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetPbftSyncPacket, {})).value();
  const auto packet6_pbft_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::PbftSyncPacket, {})).value();
  const auto packet7_pbft_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::PbftSyncPacket, {})).value();

  size_t packets_count = 0;
  const auto packet8_get_dag_sync_id = packets_count =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::GetDagSyncPacket, {})).value();

  tp.startProcessing();

  // How should packets be processed:
  // Note: To understand how are different packet types processed (concurrently without any blocking dependencies or
  // synchronously due to some blocking dependencies - depends on situation), check
  // PriorityQueue::updateDependenciesStart
  /*
    ------------------------
    --- packet0_dag_sync ---
    ------------------------
                              ------------------------
                              --- packet1_dag_sync ---
                              ------------------------
    -------------------------
    -- packet2_get_dag_sync -
    -------------------------
                              -------------------------
                              -- packet3_get_dag_sync -
                              -------------------------
    -------------------------
    - packet4_get_pbft_sync -
    -------------------------
                              -------------------------
                              - packet5_get_pbft_sync -
                              -------------------------
    ------------------------
    --- packet6_pbft_sync --
    ------------------------
                              ------------------------
                              --- packet7_pbft_sync --
                              ------------------------
                                                       ------------------------
                                                       - packet8_get_dag_sync -
                                                       ------------------------
    0......................20........................40........................60.......... time
  */

  // All packets should be already being processed after short amount of time
  std::this_thread::sleep_for(60ms + QUEUE_EMPTIED_WAIT_TRESHOLD_MS);
  EXPECT_EQ(queuesSize(tp), 0);

  // Wait until processing of all packets is finished - in some edge cases it might be little bit delayed due to locking
  EXPECT_HAPPENS({500s, 20ms}, [&](auto& ctx) {
    // Check if transactions was propagated to node0
    WAIT_EXPECT_EQ(ctx, init_data.packets_processing_info->getPacketProcessingTimesCount(), packets_count + 1)
  });

  // Check order of packets how they were processed
  const auto packets_proc_info = init_data.packets_processing_info;

  const auto packet0_dag_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet0_dag_sync_id);
  const auto packet1_dag_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet1_dag_sync_id);
  const auto packet2_get_dag_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet2_get_dag_sync_id);
  const auto packet3_get_dag_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet3_get_dag_sync_id);
  const auto packet4_get_pbft_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet4_get_pbft_sync_id);
  const auto packet5_get_pbft_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet5_get_pbft_sync_id);
  const auto packet6_pbft_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet6_pbft_sync_id);
  const auto packet7_pbft_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet7_pbft_sync_id);
  const auto packet8_get_dag_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet8_get_dag_sync_id);

  checkConcurrentProcessing({
      {packet0_dag_sync_proc_info, "packet0_dag_sync"},
      {packet2_get_dag_sync_proc_info, "packet2_get_dag_sync"},
      {packet4_get_pbft_sync_proc_info, "packet4_get_pbft_sync"},
      {packet6_pbft_sync_proc_info, "packet6_pbft_sync"},
  });

  checkConcurrentProcessing({
      {packet1_dag_sync_proc_info, "packet1_dag_sync"},
      {packet3_get_dag_sync_proc_info, "packet3_get_dag_sync"},
      {packet5_get_pbft_sync_proc_info, "packet5_get_pbft_sync"},
      {packet7_pbft_sync_proc_info, "packet7_pbft_sync"},
  });

  EXPECT_GT(packet1_dag_sync_proc_info.start_time_, packet0_dag_sync_proc_info.finish_time_);
  EXPECT_GT(packet3_get_dag_sync_proc_info.start_time_, packet2_get_dag_sync_proc_info.finish_time_);
  EXPECT_GT(packet5_get_pbft_sync_proc_info.start_time_, packet4_get_pbft_sync_proc_info.finish_time_);
  EXPECT_GT(packet7_pbft_sync_proc_info.start_time_, packet6_pbft_sync_proc_info.finish_time_);

  EXPECT_GT(packet8_get_dag_sync_proc_info.start_time_, packet3_get_dag_sync_proc_info.finish_time_);
}

// Test "peer-order blocking dependencies" related to specific (peer & order) combination:
//
// Packets types that are blocked only for processing when received from specific peer & after specific
// time (order), e.g.: new dag block packet processing is blocked until all transactions packets that were received
// before it are processed. This blocking dependency is applied only for the same peer so transaction packet from one
// peer does not block new dag block packet from another peer
TEST_F(TarcapTpTest, peer_order_blocking_deps) {
  HandlersInitData init_data = createHandlersInitData();

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  packets_handler->registerHandler<DummyTransactionPacketHandler>(init_data, "TX_PH", 20);
  packets_handler->registerHandler<DummyDagBlockPacketHandler>(init_data, "DAG_BLOCK_PH", 0);
  packets_handler->registerHandler<DummyDagSyncPacketHandler>(init_data, "SYNC_TEST_PH", 40);

  // Creates threadpool
  threadpool::PacketsThreadPool tp(10);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  // Pushes packets to the tp
  const auto packet0_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket)).value();
  const auto packet1_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket)).value();
  const auto packet2_dag_sync_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagSyncPacket)).value();
  const auto packet3_tx_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket)).value();

  size_t packets_count = 0;
  const auto packet4_dag_block_id = packets_count =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(1)}))
          .value();

  // How should packets be processed:
  // Note: To understand how are different packet types processed (concurrently without any blocking dependencies or
  // synchronously due to some blocking dependencies - depends on situation), check
  // PriorityQueue::updateDependenciesStart
  /*
    --------------
    - packet0_tx -
    --------------
    --------------
    - packet1_tx -
    --------------
    ----------------------------
    ----- packet2_dag_sync -----
    ----------------------------
    --------------
    - packet3_tx -
    --------------
                                 ---------------------
                                 - packet4_dag_block -
                                 ---------------------
    0............20.............40....................60.................. time [ms]
  */

  tp.startProcessing();

  // All packets should be already being processed after short amount of time
  std::this_thread::sleep_for(60ms + QUEUE_EMPTIED_WAIT_TRESHOLD_MS);
  EXPECT_EQ(queuesSize(tp), 0);

  // Wait until processing of all packets is finished - in some edge cases it might be little bit delayed due to locking
  EXPECT_HAPPENS({500s, 20ms}, [&](auto& ctx) {
    // Check if transactions was propagated to node0
    WAIT_EXPECT_EQ(ctx, init_data.packets_processing_info->getPacketProcessingTimesCount(), packets_count + 1)
  });

  // Check order of packets how they were processed
  const auto packets_proc_info = init_data.packets_processing_info;

  const auto packet0_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet0_tx_id);
  const auto packet1_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet1_tx_id);
  const auto packet3_tx_proc_info = packets_proc_info->getPacketProcessingTimes(packet3_tx_id);
  const auto packet4_dag_block_proc_info = packets_proc_info->getPacketProcessingTimes(packet4_dag_block_id);
  const auto packet2_dag_sync_proc_info = packets_proc_info->getPacketProcessingTimes(packet2_dag_sync_id);

  checkConcurrentProcessing({
      {packet0_tx_proc_info, "packet0_tx"},
      {packet1_tx_proc_info, "packet1_tx"},
      {packet2_dag_sync_proc_info, "packet2_dag_sync"},
      {packet3_tx_proc_info, "packet3_tx"},
  });

  EXPECT_GT(packet2_dag_sync_proc_info.finish_time_, packet0_tx_proc_info.finish_time_);
  EXPECT_GT(packet2_dag_sync_proc_info.finish_time_, packet1_tx_proc_info.finish_time_);
  EXPECT_GT(packet2_dag_sync_proc_info.finish_time_, packet3_tx_proc_info.finish_time_);

  EXPECT_GT(packet4_dag_block_proc_info.start_time_, packet2_dag_sync_proc_info.finish_time_);
}

// Test "dag-block blocking dependencies" related to dag blocks:
//
// Same dag blocks should not be processed at the same time
TEST_F(TarcapTpTest, same_dag_blks_ordering) {
  HandlersInitData init_data = createHandlersInitData();

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  packets_handler->registerHandler<DummyDagBlockPacketHandler>(init_data, "DAG_BLOCK_PH", 20);

  // Creates threadpool
  threadpool::PacketsThreadPool tp(10);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  auto dag_block = createDagBlockRlp(0);

  // Pushes packets to the tp
  const auto blk0_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {dag_block})).value();
  const auto blk1_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {dag_block})).value();
  const auto blk2_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {dag_block})).value();
  const auto blk3_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {dag_block})).value();

  size_t packets_count = 0;
  const auto blk4_id = packets_count =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {dag_block})).value();

  tp.startProcessing();

  // How should dag blocks packets be processed:
  // Same dag blocks should not be processed concurrently but one after another

  // All packets should be already being processed after short amount of time
  std::this_thread::sleep_for(200ms + QUEUE_EMPTIED_WAIT_TRESHOLD_MS);
  EXPECT_EQ(queuesSize(tp), 0);

  // Wait until processing of all packets is finished - in some edge cases it might be little bit delayed due to locking
  EXPECT_HAPPENS({500s, 20ms}, [&](auto& ctx) {
    // Check if transactions was propagated to node0
    WAIT_EXPECT_EQ(ctx, init_data.packets_processing_info->getPacketProcessingTimesCount(), packets_count + 1)
  });

  // Check order of packets how they were processed
  const auto packets_proc_info = init_data.packets_processing_info;

  const auto blk0_proc_info = packets_proc_info->getPacketProcessingTimes(blk0_id);
  const auto blk1_proc_info = packets_proc_info->getPacketProcessingTimes(blk1_id);
  const auto blk2_proc_info = packets_proc_info->getPacketProcessingTimes(blk2_id);
  const auto blk3_proc_info = packets_proc_info->getPacketProcessingTimes(blk3_id);
  const auto blk4_proc_info = packets_proc_info->getPacketProcessingTimes(blk4_id);

  checkSerialProcessing({
      {blk0_proc_info, "blk0"},
      {blk1_proc_info, "blk1"},
      {blk2_proc_info, "blk2"},
      {blk3_proc_info, "blk3"},
      {blk4_proc_info, "blk4"},
  });
}

// Test "dag-level blocking dependencies" related to dag blocks levels:
//
// Ideally only dag blocks with the same level should be processed. In reality there are situation when node receives
// dag block with smaller level than the level of blocks that are already being processed. In such case these blocks
// with smaller levels can be processed concurrently with blocks that have higher level. All new dag blocks with higher
// level than the lowest level from all the blocks that currently being processed are blocked for processing
TEST_F(TarcapTpTest, dag_blks_lvls_ordering) {
  HandlersInitData init_data = createHandlersInitData();

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  packets_handler->registerHandler<DummyDagBlockPacketHandler>(init_data, "DAG_BLOCK_PH", 20);

  // Creates threadpool
  threadpool::PacketsThreadPool tp(10);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  // Pushes packets to the tp
  const auto blk0_lvl1_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(1, 1)}))
          .value();
  const auto blk1_lvl1_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(1, 2)}))
          .value();
  const auto blk2_lvl0_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(0, 3)}))
          .value();
  const auto blk3_lvl1_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(1, 4)}))
          .value();
  const auto blk4_lvl2_id =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(2, 5)}))
          .value();

  size_t packets_count = 0;
  const auto blk5_lvl3_id = packets_count =
      tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::DagBlockPacket, {createDagBlockRlp(3, 6)}))
          .value();

  tp.startProcessing();

  // How should dag blocks packets be processed:
  // Note: To understand how are different packet types processed (concurrently without any blocking dependencies or
  // synchronously due to some blocking dependencies - depends on situation), check
  // PriorityQueue::updateDependenciesStart
  /*
    -------------
    - blk0_lvl1 -
    -------------
    -------------
    - blk1_lvl1 -
    -------------
    -------------
    - blk2_lvl0 -
    -------------
                  -------------
                  - blk3_lvl1 -
                  -------------
                                -------------
                                - blk4_lvl2 -
                                -------------
                                              -------------
                                              - blk5_lvl3 -
                                              -------------
    0...........20............40............60.............80................. time [ms]
  */

  // All packets should be already being processed after short amount of time
  std::this_thread::sleep_for(80ms + QUEUE_EMPTIED_WAIT_TRESHOLD_MS);
  EXPECT_EQ(queuesSize(tp), 0);

  // Wait until processing of all packets is finished - in some edge cases it might be little bit delayed due to locking
  EXPECT_HAPPENS({500s, 20ms}, [&](auto& ctx) {
    // Check if transactions was propagated to node0
    WAIT_EXPECT_EQ(ctx, init_data.packets_processing_info->getPacketProcessingTimesCount(), packets_count + 1)
  });

  // Check order of packets how they were processed
  const auto packets_proc_info = init_data.packets_processing_info;

  const auto blk0_lvl1_proc_info = packets_proc_info->getPacketProcessingTimes(blk0_lvl1_id);
  const auto blk1_lvl1_proc_info = packets_proc_info->getPacketProcessingTimes(blk1_lvl1_id);
  const auto blk2_lvl0_proc_info = packets_proc_info->getPacketProcessingTimes(blk2_lvl0_id);
  const auto blk3_lvl1_proc_info = packets_proc_info->getPacketProcessingTimes(blk3_lvl1_id);
  const auto blk4_lvl2_proc_info = packets_proc_info->getPacketProcessingTimes(blk4_lvl2_id);
  const auto blk5_lvl3_proc_info = packets_proc_info->getPacketProcessingTimes(blk5_lvl3_id);

  checkConcurrentProcessing({
      {blk0_lvl1_proc_info, "blk0_lvl1"},
      {blk1_lvl1_proc_info, "blk1_lvl1"},
      {blk2_lvl0_proc_info, "blk2_lvl0"},
  });

  EXPECT_GT(blk3_lvl1_proc_info.start_time_, blk2_lvl0_proc_info.finish_time_);
  EXPECT_GT(blk4_lvl2_proc_info.start_time_, blk3_lvl1_proc_info.finish_time_);
  EXPECT_GT(blk5_lvl3_proc_info.start_time_, blk4_lvl2_proc_info.finish_time_);
}

// Test threads borrowing
//
// It can happen that no packet for processing was returned during the first iteration over priority queues as there
// are limits for max total workers per each priority queue. These limits can and should be ignored in some
// scenarios... For example:
// High priority queue reached it's max workers limit, other queues have inside many blocked packets that cannot be
// currently processed concurrently and MAX_TOTAL_WORKERS_COUNT is not reached yet. In such case some threads might
// be unused. In such cases priority queues max workers limits can and should be ignored.
//
// Always keep 1 reserved thread for each priority queue at all times
TEST_F(TarcapTpTest, threads_borrowing) {
  HandlersInitData init_data = createHandlersInitData();

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  packets_handler->registerHandler<DummyVotePacketHandler>(init_data, "VOTE_PH", 100);

  // Creates threadpool
  const size_t threads_num = 10;
  threadpool::PacketsThreadPool tp(threads_num);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  // Pushes packets to the tp
  std::vector<uint64_t> pushed_packets_ids;
  for (size_t i = 0; i < threads_num; i++) {
    uint64_t packet_id = tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::VotePacket, {})).value();
    pushed_packets_ids.push_back(packet_id);
  }

  tp.startProcessing();

  // How should packets be processed:
  // Note: To understand how are different packet types processed (concurrently without any blocking dependencies or
  // synchronously due to some blocking dependencies - depends on situation), check
  // PriorityQueue::updateDependenciesStart
  //
  // Note: each queue has 1 thread reserved at all times(even if it does not do anything) and there is 10 threads in
  //       total, even with borrowing only 8 threads could be used at the same time
  /*
    ----------------
    - packet0_vote -
    ----------------
    ----------------
    - packet1_vote -
    ----------------
    ----------------
    - packet2_vote -
    ----------------

          -||-
          ...

    ----------------
    - packet7_vote -
    ----------------
                     ----------------
                     - packet8_vote -
                     ----------------
                     ----------------
                     - packet9_vote -
                     ----------------
    0..............100...............200........... time [ms]
   */

  // First 8 packets should be already processed by this time
  std::this_thread::sleep_for(100ms + 50ms /* might take longer due to threads borrowing */);
  EXPECT_LE(queuesSize(tp), 2);

  // Check order of packets how they were processed
  const auto packets_proc_info = init_data.packets_processing_info;

  // In case some packet processing is not finished yet, getPacketProcessingTimes() returns default (empty) value
  std::chrono::steady_clock::time_point default_time_point;

  // Because each queue has 1 thread reserved at all times(even if it does not do anything) and there is 10 threads in
  // total, even with borrowing only 8 threads could be used at the same time, thus last 2 packets (9th & 10th) should
  // not be processed after (100 + WAIT_TRESHOLD_MS) ms
  EXPECT_EQ(packets_proc_info->getPacketProcessingTimes(pushed_packets_ids[8]).finish_time_, default_time_point);
  EXPECT_EQ(packets_proc_info->getPacketProcessingTimes(pushed_packets_ids[9]).finish_time_, default_time_point);

  std::vector<std::pair<PacketsProcessingInfo::PacketProcessingTimes, std::string>> packets_proc_info_vec;
  for (size_t i = 0; i < threads_num - (threadpool::PacketData::PacketPriority::Count - 1); i++) {
    packets_proc_info_vec.emplace_back(packets_proc_info->getPacketProcessingTimes(pushed_packets_ids[i]),
                                       "packet" + std::to_string(pushed_packets_ids[i]) + "_vote");
  }

  // Check if first 8 pbft vote packets were processed concurrently -> threads from other queues had to be borrowed for
  // that
  checkConcurrentProcessing(packets_proc_info_vec);
}

// Test low priority queue starvation
//
// It should never happen that packets from lower priority queues are waiting to be processed until all packets from
// higher priority queues are processed
TEST_F(TarcapTpTest, low_priotity_queue_starvation) {
  HandlersInitData init_data = createHandlersInitData();

  auto packets_handler = std::make_shared<tarcap::PacketsHandler>();
  // Handler for packet from high priority queue
  packets_handler->registerHandler<DummyVotePacketHandler>(init_data, "VOTE_PH", 20);

  // Handler for packet from mid priority queue
  packets_handler->registerHandler<DummyTransactionPacketHandler>(init_data, "TX_PH", 20);

  // Handler for packet from low priority queue
  packets_handler->registerHandler<DummyStatusPacketHandler>(init_data, "STATUS_PH", 20);

  // Creates threadpool
  size_t threads_num = 10;
  threadpool::PacketsThreadPool tp(threads_num);
  tp.setPacketsHandlers(TARAXA_NET_VERSION, packets_handler);

  // Push 10x more packets for each prioriy queue than max tp capacity to make sure that tp wont be able to process all
  // packets from each queue concurrently -> many packets will be waiting due to max threads num reached for specific
  // priority queues
  for (size_t i = 0; i < 2 * 10 * threads_num; i++) {
    tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::VotePacket, {})).value();
    tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::TransactionPacket, {})).value();
  }

  // Push a few packets low priority packets
  for (size_t i = 0; i < 4; i++) {
    tp.push(createPacket(init_data.copySender(), SubprotocolPacketType::StatusPacket, {})).value();
  }

  tp.startProcessing();

  // How should packets be processed:
  // Note: To understand how are different packet types processed (concurrently without any blocking dependencies or
  // synchronously due to some blocking dependencies - depends on situation), check
  // PriorityQueue::updateDependenciesStart In this test are max concurrent processing limits for queues reached, so
  // when we have 10 threads in thredpool:
  // - 4 is limit for High priority queue - VotePacket
  // - 4 is limit for Mid priority queue - TransactionPacket
  // - 3 is limit for Low priority queue - StatusPacket, but because max total limit (10) is always checked first
  // , low priority queue wont be able to use more than 2 threads concurrently
  /*
    ----------------
    - packet0_vote -
    ----------------
    ----------------
    - packet1_vote -
    ----------------
    ----------------
    - packet2_vote -
    ----------------
    ----------------
    - packet3_vote -
    ----------------
    ----------------
    -- packet4_tx --
    ----------------
    ----------------
    -- packet5_tx --
    ----------------
    ----------------
    -- packet6_tx --
    ----------------
    ----------------
    -- packet7_tx --
    ----------------

         ....
      votes and tx packets are processed concurrently 4 at a time until all of them are processed


    ------------------
    - packet400_test -
    ------------------
    ------------------
    - packet401_test -
    ------------------
                       ------------------
                       - packet402_test -
                       ------------------
                       ------------------
                       - packet403_test -
                       ------------------
    0.................20.................40................... time [ms]
  */

  std::this_thread::sleep_for(40ms + QUEUE_EMPTIED_WAIT_TRESHOLD_MS);

  const auto [high_priority_queue_size, mid_priority_queue_size, low_priority_queue_size] = tp.getQueueSize();

  EXPECT_GT(high_priority_queue_size, 0);
  EXPECT_GT(mid_priority_queue_size, 0);
  EXPECT_EQ(low_priority_queue_size, 0);
}

}  // namespace taraxa::core_tests

int main(int argc, char** argv) {
  using namespace taraxa;

  auto logging = logger::createDefaultLoggingConfig();

  // Set this to debug to see log msgs
  logging.verbosity = logger::Verbosity::Debug;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}