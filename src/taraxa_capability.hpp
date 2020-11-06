#ifndef TARAXA_NODE_TARAXA_CAPABILITY_HPP
#define TARAXA_NODE_TARAXA_CAPABILITY_HPP
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>

#include <chrono>
#include <set>
#include <thread>

#include "config.hpp"
#include "dag_block.hpp"
#include "transaction.hpp"
#include "util.hpp"
#include "vote.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace dev::p2p;

enum SubprotocolPacketType : ::byte {

  StatusPacket = 0x0,
  NewBlockPacket,
  NewBlockHashPacket,
  GetNewBlockPacket,
  GetBlockPacket,
  BlockPacket,
  GetBlocksPacket,
  BlocksPacket,
  GetLeavesBlocksPacket,
  LeavesBlocksPacket,
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

struct InvalidDataException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

class TaraxaPeer : public boost::noncopyable {
 public:
  TaraxaPeer() : known_blocks_(10000, 1000), known_pbft_blocks_(10000, 1000), known_votes_(10000, 1000), known_transactions_(100000, 10000) {}
  explicit TaraxaPeer(NodeID id)
      : m_id(id), known_blocks_(10000, 1000), known_pbft_blocks_(10000, 1000), known_votes_(10000, 1000), known_transactions_(100000, 10000) {}

  bool isBlockKnown(blk_hash_t const &_hash) const { return known_blocks_.count(_hash); }
  void markBlockAsKnown(blk_hash_t const &_hash) { known_blocks_.insert(_hash); }

  bool isTransactionKnown(trx_hash_t const &_hash) const { return known_transactions_.count(_hash); }
  void markTransactionAsKnown(trx_hash_t const &_hash) { known_transactions_.insert(_hash); }

  void clearAllKnownBlocksAndTransactions() {
    known_transactions_.clear();
    known_blocks_.clear();
  }

  // PBFT
  bool isVoteKnown(vote_hash_t const &_hash) const { return known_votes_.count(_hash); }
  void markVoteAsKnown(vote_hash_t const &_hash) { known_votes_.insert(_hash); }

  bool isPbftBlockKnown(blk_hash_t const &_hash) const { return known_pbft_blocks_.count(_hash); }
  void markPbftBlockAsKnown(blk_hash_t const &_hash) { known_pbft_blocks_.insert(_hash); }

  bool checkStatus(uint16_t max_check_count) {
    status_check_count_++;
    return status_check_count_ <= max_check_count;
  }

  void statusReceived() { status_check_count_ = 0; }

  std::map<uint64_t, std::map<blk_hash_t, std::pair<DagBlock, std::vector<Transaction>>>> sync_blocks_;
  bool syncing_ = false;
  uint64_t dag_level_ = 0;
  uint64_t pbft_chain_size_ = 0;
  uint64_t pbft_round_ = 1;

 private:
  ExpirationCache<blk_hash_t> known_blocks_;
  ExpirationCache<trx_hash_t> known_transactions_;
  // PBFT
  ExpirationCache<vote_hash_t> known_votes_;  // for peers
  ExpirationCache<blk_hash_t> known_pbft_blocks_;

  NodeID m_id;

  uint16_t status_check_count_ = 0;
};

class TaraxaCapability : public CapabilityFace, public Worker {
 public:
  TaraxaCapability(Host &_host, NetworkConfig &_conf, std::string const &genesis, bool const &performance_log, addr_t node_addr,
                   std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                   std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<BlockManager> blk_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, uint32_t lambda_ms_min)
      : Worker("taraxa"),
        host_(_host),
        conf_(_conf),
        genesis_(genesis),
        performance_log_(performance_log),
        urng_(std::mt19937_64(std::random_device()())),
        delay_rng_(std::mt19937(std::random_device()())),
        random_dist_(std::uniform_int_distribution<std::mt19937::result_type>(90, 110)),
        db_(db),
        pbft_mgr_(pbft_mgr),
        pbft_chain_(pbft_chain),
        vote_mgr_(vote_mgr),
        dag_mgr_(dag_mgr),
        blk_mgr_(blk_mgr),
        trx_mgr_(trx_mgr),
        lambda_ms_min_(lambda_ms_min) {
    LOG_OBJECTS_CREATE("TARCAP");
    LOG_OBJECTS_CREATE_SUB("PBFTSYNC", pbft_sync);
    LOG_OBJECTS_CREATE_SUB("DAGSYNC", dag_sync);
    LOG_OBJECTS_CREATE_SUB("NEXTVOTESSYNC", next_votes_sync);
    LOG_OBJECTS_CREATE_SUB("DAGPRP", dag_prp);
    LOG_OBJECTS_CREATE_SUB("TRXPRP", trx_prp);
    LOG_OBJECTS_CREATE_SUB("PBFTPRP", pbft_prp);
    LOG_OBJECTS_CREATE_SUB("VOTEPRP", vote_prp);
    LOG_OBJECTS_CREATE_SUB("NETPER", net_per);
    for (uint8_t it = 0; it != PacketCount; it++) {
      packet_count[it] = 0;
      packet_size[it] = 0;
      unique_packet_count[it] = 0;
    }
  }
  virtual ~TaraxaCapability() = default;
  std::string name() const override { return "taraxa"; }
  unsigned version() const override { return 1; }
  unsigned messageCount() const override { return PacketCount; }

  void onStarting() override;
  void onStopping() override {
    stopped_ = true;
    if (conf_.network_simulated_delay > 0) io_service_.stop();
    blk_mgr_ = nullptr;
    vote_mgr_ = nullptr;
    dag_mgr_ = nullptr;
    trx_mgr_ = nullptr;
  }

  void onConnect(NodeID const &_nodeID, u256 const &) override;
  void syncPeerPbft(NodeID const &_nodeID, unsigned long height_to_sync);
  void restartSyncingPbft(bool force = false);
  void delayedPbftSync(NodeID _nodeID, int counter);
  std::pair<bool, blk_hash_t> checkDagBlockValidation(DagBlock const &block);
  bool interpretCapabilityPacket(NodeID const &_nodeID, unsigned _id, RLP const &_r) override;
  bool interpretCapabilityPacketImpl(NodeID const &_nodeID, unsigned _id, RLP const &_r);
  void onDisconnect(NodeID const &_nodeID) override;
  void sendTestMessage(NodeID const &_id, int _x);
  void sendStatus(NodeID const &_id, bool _initial);
  void onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions);
  void onNewBlockVerified(DagBlock const &block);
  void onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork);
  vector<NodeID> selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate);
  vector<NodeID> getAllPeers() const;
  Json::Value getStatus() const;
  std::pair<std::vector<NodeID>, std::vector<NodeID>> randomPartitionPeers(std::vector<NodeID> const &_peers, std::size_t _number);
  std::pair<int, int> retrieveTestData(NodeID const &_id);
  void sendBlock(NodeID const &_id, taraxa::DagBlock block, bool newBlock);
  void sendSyncedMessage();
  void sendSyncedResponseMessage(NodeID const &_id);
  void sendBlocks(NodeID const &_id, std::vector<std::shared_ptr<DagBlock>> blocks);
  void sendLeavesBlocks(NodeID const &_id, std::vector<std::string> blocks);
  void sendBlockHash(NodeID const &_id, taraxa::DagBlock block);
  void requestBlock(NodeID const &_id, blk_hash_t hash, bool newBlock);
  void requestPendingDagBlocks(NodeID const &_id, level_t level);
  void requestLeavesDagBlocks(NodeID const &_id);
  void sendTransactions(NodeID const &_id, std::vector<taraxa::bytes> const &transactions);
  bool processSyncDagBlocks(NodeID const &_id);

  std::map<blk_hash_t, taraxa::DagBlock> getBlocks();
  std::map<trx_hash_t, taraxa::Transaction> getTransactions();

  void doBackgroundWork();
  void sendTransactions();
  std::string packetToPacketName(byte const &packet) const;

  // PBFT
  void onNewPbftVote(taraxa::Vote const &vote);
  void sendPbftVote(NodeID const &_id, taraxa::Vote const &vote);
  void onNewPbftBlock(taraxa::PbftBlock const &pbft_block);
  void sendPbftBlock(NodeID const &_id, taraxa::PbftBlock const &pbft_block, uint64_t const &pbft_chain_size);
  void requestPbftBlocks(NodeID const &_id, size_t height_to_sync);
  void sendPbftBlocks(NodeID const &_id, size_t height_to_sync, size_t blocks_to_transfer);
  void syncPbftNextVotes(uint64_t const pbft_round);
  void requestPbftNextVotes(NodeID const &peerID, uint64_t const pbft_round);
  void sendPbftNextVotes(NodeID const &peerID);

  // Peers
  std::shared_ptr<TaraxaPeer> getPeer(NodeID const &node_id);
  unsigned int getPeersCount();
  void erasePeer(NodeID const &node_id);
  void insertPeer(NodeID const &node_id, std::shared_ptr<TaraxaPeer> const &peer);

  bool syncing_ = false;
  bool requesting_pending_dag_blocks_ = false;
  NodeID requesting_pending_dag_blocks_node_id_;

 private:
  Host &host_;
  std::unordered_map<NodeID, int> cnt_received_messages_;
  std::unordered_map<NodeID, int> test_sums_;

  std::set<blk_hash_t> verified_blocks_;
  std::condition_variable condition_for_verified_blocks_;
  std::mutex mtx_for_verified_blocks;

  // Only used for testing without the full node set
  std::map<blk_hash_t, taraxa::DagBlock> test_blocks_;
  std::map<trx_hash_t, Transaction> test_transactions_;

  std::set<blk_hash_t> block_requestes_set_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<BlockManager> blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  uint32_t lambda_ms_min_;

  std::unordered_map<NodeID, std::shared_ptr<TaraxaPeer>> peers_;
  mutable boost::shared_mutex peers_mutex_;
  NetworkConfig conf_;
  boost::thread_group delay_threads_;
  boost::asio::io_service io_service_;
  std::shared_ptr<boost::asio::io_service::work> io_work_;
  unsigned long peer_syncing_pbft_chain_size_ = 1;
  uint64_t pbft_sync_period_ = 1;
  NodeID peer_syncing_pbft;
  std::string genesis_;
  bool performance_log_;
  mutable std::mt19937_64 urng_;  // Mersenne Twister psuedo-random number generator
  std::mt19937 delay_rng_;
  bool stopped_ = false;
  std::uniform_int_distribution<std::mt19937::result_type> random_dist_;
  uint16_t check_status_interval_ = 0;

  std::map<uint8_t, uint64_t> packet_count;
  std::map<uint8_t, uint64_t> packet_size;
  std::map<uint8_t, uint64_t> unique_packet_count;
  uint64_t received_trx_count = 0;
  uint64_t unique_received_trx_count = 0;

  LOG_OBJECTS_DEFINE;
  LOG_OBJECTS_DEFINE_SUB(pbft_sync);
  LOG_OBJECTS_DEFINE_SUB(dag_sync);
  LOG_OBJECTS_DEFINE_SUB(next_votes_sync);
  LOG_OBJECTS_DEFINE_SUB(dag_prp);
  LOG_OBJECTS_DEFINE_SUB(trx_prp);
  LOG_OBJECTS_DEFINE_SUB(pbft_prp);
  LOG_OBJECTS_DEFINE_SUB(vote_prp);
  LOG_OBJECTS_DEFINE_SUB(net_per);
};
}  // namespace taraxa
#endif