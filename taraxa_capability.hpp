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
#include "dag_block.hpp"
#include "full_node.hpp"
#include "transaction.hpp"
#include "util.hpp"

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace taraxa {

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
  NewPbftBlockPacket,
  GetPbftBlockPacket,
  PbftBlockPacket,
  SyncedPacket,
  PacketCount
};

class TaraxaPeer : public boost::noncopyable {
 public:
  TaraxaPeer()
      : known_blocks_(10000, 1000),
        known_pbft_blocks_(10000, 1000),
        known_votes_(10000, 1000),
        known_transactions_(100000, 10000) {}
  TaraxaPeer(NodeID id)
      : m_id(id),
        known_blocks_(10000, 1000),
        known_pbft_blocks_(10000, 1000),
        known_votes_(10000, 1000),
        known_transactions_(100000, 10000) {
    setLastMessage();
  }

  bool isBlockKnown(blk_hash_t const &_hash) const {
    return known_blocks_.count(_hash);
  }
  void markBlockAsKnown(blk_hash_t const &_hash) {
    known_blocks_.insert(_hash);
  }

  bool isTransactionKnown(trx_hash_t const &_hash) const {
    return known_transactions_.count(_hash);
  }
  void markTransactionAsKnown(trx_hash_t const &_hash) {
    known_transactions_.insert(_hash);
  }

  void clearAllKnownBlocksAndTransactions() {
    known_transactions_.clear();
    known_blocks_.clear();
  }

  // PBFT
  bool isVoteKnown(vote_hash_t const &_hash) const {
    return known_votes_.count(_hash);
  }
  void markVoteAsKnown(vote_hash_t const &_hash) { known_votes_.insert(_hash); }

  bool isPbftBlockKnown(blk_hash_t const &_hash) const {
    return known_pbft_blocks_.count(_hash);
  }
  void markPbftBlockAsKnown(blk_hash_t const &_hash) {
    known_pbft_blocks_.insert(_hash);
  }

  time_t lastAsk() const { return last_ask_; }
  time_t lastMessageTime() const { return last_message_time_; }

  bool asking() const { return asking_; }
  void setAsking(bool asking) {
    asking_ = asking;
    if (asking)
      last_ask_ =
          std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  }
  void setLastMessage() {
    last_message_time_ =
        std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  }

  std::map<uint64_t,
           std::map<blk_hash_t, std::pair<DagBlock, std::vector<Transaction>>>>
      sync_blocks_;
  bool syncing_ = false;
  uint64_t dag_level_ = 0;
  uint64_t pbft_chain_size_ = 0;

 private:
  ExpirationCache<blk_hash_t> known_blocks_;
  ExpirationCache<trx_hash_t> known_transactions_;
  // PBFT
  ExpirationCache<vote_hash_t> known_votes_;  // for peers
  ExpirationCache<blk_hash_t> known_pbft_blocks_;

  NodeID m_id;

  // Did we ask peer for something
  bool asking_ = false;
  // When we asked for it. Allows a time out.
  time_t last_ask_ = 0;
  time_t last_message_time_ = 0;
};

class TaraxaCapability : public CapabilityFace, public Worker {
 public:
  TaraxaCapability(Host &_host, NetworkConfig &_conf,
                   std::string const &genesis, bool const &performance_log)
      : Worker("taraxa"),
        host_(_host),
        conf_(_conf),
        genesis_(genesis),
        performance_log_(performance_log) {
    std::random_device seed;
    urng_ = std::mt19937_64(seed());
    delay_rng_ = std::mt19937(seed());
    random_dist_ =
        std::uniform_int_distribution<std::mt19937::result_type>(90, 110);
  }
  virtual ~TaraxaCapability() = default;
  std::string name() const override { return "taraxa"; }
  unsigned version() const override { return 1; }
  unsigned messageCount() const override { return PacketCount; }

  void onStarting() override;
  void onStopping() override {
    stopped_ = true;
    if (conf_.network_simulated_delay > 0) io_service_.stop();
  }

  void onConnect(NodeID const &_nodeID, u256 const &) override;
  void syncPeerPbft(NodeID const &_nodeID, unsigned long height_to_sync);
  void restartSyncingPbft(bool force = false);
  void delayedPbftSync(NodeID _nodeID, int counter);
  std::pair<bool, blk_hash_t> checkTipsandPivot(DagBlock const &block);
  bool interpretCapabilityPacket(NodeID const &_nodeID, unsigned _id,
                                 RLP const &_r) override;
  bool interpretCapabilityPacketImpl(NodeID const &_nodeID, unsigned _id,
                                     RLP const &_r);
  void onDisconnect(NodeID const &_nodeID) override;
  void sendTestMessage(NodeID const &_id, int _x);
  void sendStatus(NodeID const &_id);
  void onNewBlockReceived(DagBlock block,
                          std::vector<Transaction> transactions);
  void onNewBlockVerified(DagBlock block);
  void onNewTransactions(std::vector<taraxa::bytes> const &transactions,
                         bool fromNetwork);
  vector<NodeID> selectPeers(
      std::function<bool(TaraxaPeer const &)> const &_predicate);
  vector<NodeID> getAllPeers() const;
  Json::Value getStatus() const;
  std::pair<std::vector<NodeID>, std::vector<NodeID>> randomPartitionPeers(
      std::vector<NodeID> const &_peers, std::size_t _number);
  std::pair<int, int> retrieveTestData(NodeID const &_id);
  void sendBlock(NodeID const &_id, taraxa::DagBlock block, bool newBlock);
  void sendSyncedMessage();
  void sendBlocks(NodeID const &_id,
                  std::vector<std::shared_ptr<DagBlock>> blocks);
  void sendLeavesBlocks(NodeID const &_id, std::vector<std::string> blocks);
  void sendBlockHash(NodeID const &_id, taraxa::DagBlock block);
  void requestBlock(NodeID const &_id, blk_hash_t hash, bool newBlock);
  void requestPendingDagBlocks(NodeID const &_id, level_t level);
  void requestLeavesDagBlocks(NodeID const &_id);
  void sendTransactions(NodeID const &_id,
                        std::vector<taraxa::bytes> const &transactions);
  bool processSyncDagBlocks(NodeID const &_id);

  std::map<blk_hash_t, taraxa::DagBlock> getBlocks();
  std::map<trx_hash_t, taraxa::Transaction> getTransactions();
  void setFullNode(std::shared_ptr<FullNode> full_node);

  void doBackgroundWork();
  std::string packetToPacketName(byte const &packet);

  // PBFT
  void onNewPbftVote(taraxa::Vote const &vote);
  void sendPbftVote(NodeID const &_id, taraxa::Vote const &vote);
  void onNewPbftBlock(taraxa::PbftBlock const &pbft_block);
  void sendPbftBlock(NodeID const &_id, taraxa::PbftBlock const &pbft_block,
                     uint64_t const &pbft_chain_size);
  void requestPbftBlocks(NodeID const &_id, size_t height_to_sync);
  void sendPbftBlocks(NodeID const &_id, size_t height_to_sync,
                      size_t blocks_to_transfer);

  // Peers
  std::shared_ptr<TaraxaPeer> getPeer(NodeID const &node_id);
  unsigned int getPeersCount();
  void erasePeer(NodeID const &node_id);
  void insertPeer(NodeID const &node_id,
                  std::shared_ptr<TaraxaPeer> const &peer);

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

  std::weak_ptr<FullNode> full_node_;

  std::unordered_map<NodeID, std::shared_ptr<TaraxaPeer>> peers_;
  mutable boost::shared_mutex peers_mutex_;
  NetworkConfig conf_;
  boost::thread_group delay_threads_;
  boost::asio::io_service io_service_;
  std::shared_ptr<boost::asio::io_service::work> io_work_;
  unsigned long max_peer_level_ = 0;
  unsigned long max_peer_pbft_chain_size_ = 1;
  uint64_t pbft_sync_height_ = 1;
  NodeID peer_syncing_pbft;
  std::string genesis_;
  bool performance_log_;
  mutable std::mt19937_64
      urng_;  // Mersenne Twister psuedo-random number generator
  std::mt19937 delay_rng_;
  bool stopped_ = false;
  std::uniform_int_distribution<std::mt19937::result_type> random_dist_;

  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "TARCAP")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TARCAP")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TARCAP")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TARCAP")};
  dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "TARCAP")};
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TARCAP")};
  dev::Logger log_perf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "NETPER")};

  dev::Logger log_si_pbft_sync_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PBFTSYNC")};
  dev::Logger log_wr_pbft_sync_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFTSYNC")};
  dev::Logger log_nf_pbft_sync_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFTSYNC")};
  dev::Logger log_dg_pbft_sync_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PBFTSYNC")};
  dev::Logger log_tr_pbft_sync_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PBFTSYNC")};
  dev::Logger log_er_pbft_sync_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFTSYNC")};

  dev::Logger log_wr_dag_sync_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "DAGSYNC")};
  dev::Logger log_nf_dag_sync_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "DAGSYNC")};
  dev::Logger log_dg_dag_sync_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "DAGSYNC")};
  dev::Logger log_tr_dag_sync_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "DAGSYNC")};
  dev::Logger log_er_dag_sync_{
      dev::createLogger(dev::Verbosity::VerbosityError, "DAGSYNC")};

  dev::Logger log_wr_trx_prp_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TRXPRP")};
  dev::Logger log_nf_trx_prp_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TRXPRP")};
  dev::Logger log_dg_trx_prp_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TRXPRP")};
  dev::Logger log_tr_trx_prp_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "TRXPRP")};
  dev::Logger log_er_trx_prp_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TRXPRP")};

  dev::Logger log_wr_dag_prp_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "DAGPRP")};
  dev::Logger log_nf_dag_prp_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "DAGPRP")};
  dev::Logger log_dg_dag_prp_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "DAGPRP")};
  dev::Logger log_tr_dag_prp_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "DAGPRP")};
  dev::Logger log_er_dag_prp_{
      dev::createLogger(dev::Verbosity::VerbosityError, "DAGPRP")};

  dev::Logger log_wr_pbft_prp_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFTPRP")};
  dev::Logger log_nf_pbft_prp_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFTPRP")};
  dev::Logger log_dg_pbft_prp_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PBFTPRP")};
  dev::Logger log_tr_pbft_prp_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PBFTPRP")};
  dev::Logger log_er_pbft_prp_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFTPRP")};

  dev::Logger log_wr_vote_prp_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "VOTEPRP")};
  dev::Logger log_nf_vote_prp_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "VOTEPRP")};
  dev::Logger log_dg_vote_prp_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "VOTEPRP")};
  dev::Logger log_tr_vote_prp_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "VOTEPRP")};
  dev::Logger log_er_vote_prp_{
      dev::createLogger(dev::Verbosity::VerbosityError, "VOTEPRP")};
};
}  // namespace taraxa
#endif