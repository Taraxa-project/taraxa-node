#pragma once

#include <libp2p/Capability.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>

#include <chrono>
#include <set>
#include <thread>

#include "config/config.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block_manager.hpp"
#include "packets_stats.hpp"
#include "syncing_state.hpp"
#include "taraxa_peer.hpp"
#include "transaction_manager/transaction.hpp"
#include "util/thread_pool.hpp"
#include "util/util.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace dev::p2p;

enum SubprotocolPacketType : ::byte {

  StatusPacket = 0x0,
  NewBlockPacket,
  NewBlockHashPacket,
  GetNewBlockPacket,
  GetBlocksPacket,
  BlocksPacket,
  TransactionPacket,
  TestPacket,
  PbftVotePacket,
  GetPbftNextVotes,
  PbftNextVotesPacket,
  NewPbftBlockPacket,
  GetPbftBlockPacket,
  PbftBlockPacket,
  SyncedPacket,
  SyncedResponsePacket,
  PacketCount
};

enum GetBlocksPacketRequestType : ::byte {

  MissingHashes = 0x0,
  KnownHashes
};
struct TaraxaCapability : virtual CapabilityFace {
  TaraxaCapability(weak_ptr<Host> _host, NetworkConfig const &_conf, std::shared_ptr<DbStorage> db = {},
                   std::shared_ptr<PbftManager> pbft_mgr = {}, std::shared_ptr<PbftChain> pbft_chain = {},
                   std::shared_ptr<VoteManager> vote_mgr = {},
                   std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr = {},
                   std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<DagBlockManager> dag_blk_mgr = {},
                   std::shared_ptr<TransactionManager> trx_mgr = {}, addr_t const &node_addr = {});

  virtual ~TaraxaCapability() { stop(); }

  std::string name() const override { return "taraxa"; }
  unsigned version() const override;
  unsigned messageCount() const override { return PacketCount; }
  void onConnect(weak_ptr<Session> session, u256 const &) override;
  void interpretCapabilityPacket(weak_ptr<Session> session, unsigned _id, RLP const &_r) override;
  void onDisconnect(NodeID const &_nodeID) override;

  // TODO remove managing thread pool inside this class
  void start() {
    tp_.start();
    syncing_tp_.start();
  }
  void stop() {
    tp_.stop();
    syncing_tp_.stop();
  }

  bool sealAndSend(NodeID const &nodeID, unsigned packet_type, RLPStream rlp);
  bool pbft_syncing() const { return syncing_state_.is_pbft_syncing(); }
  uint64_t syncTimeSeconds() const { return summary_interval_ms_ * syncing_interval_count_ / 1000; };
  void syncPeerPbft(unsigned long height_to_sync);
  void restartSyncingPbft(bool force = false);
  void delayedPbftSync(int counter);
  std::pair<bool, std::vector<blk_hash_t>> checkDagBlockValidation(DagBlock const &block);
  void requestBlocks(const NodeID &_nodeID, std::vector<blk_hash_t> const &blocks,
                     GetBlocksPacketRequestType mode = MissingHashes);
  void interpretCapabilityPacketImpl(NodeID const &_nodeID, unsigned _id, RLP const &_r, PacketStats &packet_stats);
  void sendTestMessage(NodeID const &_id, int _x, std::vector<char> const &data);
  bool sendStatus(NodeID const &_id, bool _initial);
  void onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions);
  void onNewBlockVerified(DagBlock const &block);
  void onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork);
  vector<NodeID> selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate);
  vector<NodeID> getAllPeers() const;
  vector<NodeID> getAllPendingPeers() const;
  Json::Value getStatus() const;
  std::pair<std::vector<NodeID>, std::vector<NodeID>> randomPartitionPeers(std::vector<NodeID> const &_peers,
                                                                           std::size_t _number);
  std::pair<int, int> retrieveTestData(NodeID const &_id);
  void sendBlock(NodeID const &_id, taraxa::DagBlock block);
  void sendSyncedMessage();
  void sendBlocks(NodeID const &_id, std::vector<std::shared_ptr<DagBlock>> blocks);
  void sendBlockHash(NodeID const &_id, taraxa::DagBlock block);
  void requestBlock(NodeID const &_id, blk_hash_t hash);
  void requestPendingDagBlocks();
  void sendTransactions(NodeID const &_id, std::vector<taraxa::bytes> const &transactions);

  std::map<blk_hash_t, taraxa::DagBlock> getBlocks();
  std::map<trx_hash_t, taraxa::Transaction> getTransactions();

  uint64_t getSimulatedNetworkDelay(const RLP &packet_rlp, const NodeID &nodeID);

  void checkLiveness();
  void logNodeStats();
  void logPacketsStats();
  void sendTransactions();
  std::string packetTypeToString(unsigned int _packetType) const override;

  // PBFT
  void onNewPbftVote(taraxa::Vote const &vote);
  void sendPbftVote(NodeID const &peerID, taraxa::Vote const &vote);
  void onNewPbftBlock(taraxa::PbftBlock const &pbft_block);
  void sendPbftBlock(NodeID const &peerID, taraxa::PbftBlock const &pbft_block, uint64_t const &pbft_chain_size);
  void requestPbftBlocks(NodeID const &peerID, size_t height_to_sync);
  void sendPbftBlocks(NodeID const &peerID, size_t height_to_sync, size_t blocks_to_transfer);
  void syncPbftNextVotes(uint64_t const pbft_round, size_t const pbft_previous_round_next_votes_size);
  void requestPbftNextVotes(NodeID const &peerID, uint64_t const pbft_round,
                            size_t const pbft_previous_round_next_votes_size);
  void sendPbftNextVotes(NodeID const &peerID, std::vector<Vote> const &send_next_votes_bundle);
  void broadcastPreviousRoundNextVotesBundle();

  // Peers
  std::shared_ptr<TaraxaPeer> getPeer(NodeID const &node_id);
  std::shared_ptr<TaraxaPeer> getPendingPeer(NodeID const &node_id);
  unsigned int getPeersCount();
  void erasePeer(NodeID const &node_id);
  std::shared_ptr<TaraxaPeer> addPendingPeer(NodeID const &node_id);
  std::shared_ptr<TaraxaPeer> setPeerAsReadyToSendMessages(NodeID const &node_id, std::shared_ptr<TaraxaPeer> peer);

 private:
  void handle_read_exception(weak_ptr<Session> session, unsigned _id);

  weak_ptr<Host> host_;
  NodeID node_id_;
  util::ThreadPool tp_{1, false};

  util::ThreadPool syncing_tp_{1, false};
  SyncingState syncing_state_;

  std::unordered_map<NodeID, int> cnt_received_messages_;
  std::unordered_map<NodeID, int> test_sums_;

  // Only used for testing without the full node set
  std::map<blk_hash_t, taraxa::DagBlock> test_blocks_;
  std::map<trx_hash_t, Transaction> test_transactions_;

  std::set<blk_hash_t> block_requestes_set_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  uint32_t lambda_ms_min_;

  std::unordered_map<NodeID, std::shared_ptr<TaraxaPeer>> peers_;
  std::unordered_map<NodeID, std::shared_ptr<TaraxaPeer>> pending_peers_;
  mutable boost::shared_mutex peers_mutex_;
  NetworkConfig conf_;
  std::string genesis_;
  mutable std::mt19937_64 urng_;  // Mersenne Twister psuedo-random number generator
  std::mt19937 delay_rng_;
  std::uniform_int_distribution<std::mt19937::result_type> random_dist_;
  uint16_t check_alive_interval_ = 0;

  uint64_t received_trx_count = 0;
  uint64_t unique_received_trx_count = 0;

  // Node stats info history
  uint64_t summary_interval_ms_ = 30000;
  level_t local_max_level_in_dag_prev_interval_ = 0;
  uint64_t local_pbft_round_prev_interval_ = 0;
  uint64_t local_chain_size_prev_interval_ = 0;
  uint64_t local_pbft_sync_period_prev_interval_ = 0;
  uint64_t syncing_interval_count_ = 0;
  uint64_t intervals_in_sync_since_launch = 0;
  uint64_t intervals_syncing_since_launch = 0;
  uint64_t syncing_stalled_interval_count_ = 0;

  PacketsStats sent_packets_stats_;
  PacketsStats received_packets_stats_;

  const uint32_t MAX_PACKET_SIZE = 15 * 1024 * 1024;  // 15 MB -> 15 * 1024 * 1024 B
  const uint16_t MAX_CHECK_ALIVE_COUNT = 5;

  // Only allow up to 2 nodes syncing from our node
  const uint16_t MAX_SYNCING_NODES = 2;

  // If there are more than 10 packets in queue to be processed syncing will be delayed or node disconnected in queue
  // not cleared in defined time
  const uint16_t MAX_NETWORK_QUEUE_TO_DROP_SYNCING = 10;
  const uint16_t MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS = 2000;

  static constexpr uint16_t INITIAL_STATUS_PACKET_ITEM_COUNT = 10;

  LOG_OBJECTS_DEFINE
  LOG_OBJECTS_DEFINE_SUB(pbft_sync)
  LOG_OBJECTS_DEFINE_SUB(dag_sync)
  LOG_OBJECTS_DEFINE_SUB(next_votes_sync)
  LOG_OBJECTS_DEFINE_SUB(dag_prp)
  LOG_OBJECTS_DEFINE_SUB(trx_prp)
  LOG_OBJECTS_DEFINE_SUB(pbft_prp)
  LOG_OBJECTS_DEFINE_SUB(vote_prp)
  LOG_OBJECTS_DEFINE_SUB(net_per)
  LOG_OBJECTS_DEFINE_SUB(summary)
};
}  // namespace taraxa
