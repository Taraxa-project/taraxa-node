#ifndef TARAXA_CAPABILITY_HPP
#define TARAXA_CAPABILITY_HPP
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
#include "visitor.hpp"

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace taraxa {

enum SubprotocolPacketType : ::byte {
  NewBlockPacket = 0x0,
  NewBlockHashPacket,
  GetNewBlockPacket,
  GetBlockPacket,
  BlockPacket,
  GetBlockChildrenPacket,
  BlockChildrenPacket,
  TransactionPacket,
  TestPacket,
  PbftVotePacket,
  PbftBlockPacket,
  PbftPivotBlockPacket, // TODO: may not need
  PbftScheduleBlockPacket, // TODO: may not need
  PacketCount
};

enum PeerState { Idle = 0, Syncing };

class TaraxaPeer {
 public:
  TaraxaPeer() {}
  TaraxaPeer(NodeID id) : m_id(id), m_state(Idle) {}

  bool isBlockKnown(blk_hash_t const &_hash) const {
    return m_knownBlocks.count(_hash);
  }
  void markBlockAsKnown(blk_hash_t const &_hash) {
    m_knownBlocks.insert(_hash);
  }
  void clearKnownBlocks() { m_knownBlocks.clear(); }

  bool isTransactionKnown(trx_hash_t const &_hash) const {
    return m_knownTransactions.count(_hash);
  }
  void markTransactionAsKnown(trx_hash_t const &_hash) {
    m_knownTransactions.insert(_hash);
  }
  void clearKnownTransactions() { m_knownTransactions.clear(); }

  // PBFT
  bool isVoteKnown(sig_hash_t const &_hash) const {
    return m_knownVotes.count(_hash);
  }
  void markVoteAsKnown(sig_hash_t const &_hash) { m_knownVotes.insert(_hash); }
  void clearKnownVotes() { m_knownVotes.clear(); }

  bool isPbftBlockKnown(blk_hash_t const &_hash) const {
  	return m_knownPbftBlocks.count(_hash);
  }
  void markPbftBlockAsKnown(blk_hash_t const &_hash) {
  	m_knownPbftBlocks.insert(_hash);
  }
  void cleanKnownPbftBlocks() { m_knownPbftBlocks.clear(); }

  std::map<blk_hash_t, std::pair<DagBlock, std::vector<Transaction>>>
      m_syncBlocks;
  blk_hash_t m_lastRequest;
  PeerState m_state;

 private:
  std::set<blk_hash_t> m_knownBlocks;
  std::set<trx_hash_t> m_knownTransactions;
  // PBFT
  std::set<sig_hash_t> m_knownVotes;
  std::set<blk_hash_t> m_knownPbftBlocks;

  NodeID m_id;
};

class TaraxaCapability : public CapabilityFace, public Worker {
 public:
  TaraxaCapability(Host &_host, uint16_t network_simulated_delay = 0)
      : Worker("taraxa"),
        host_(_host),
        network_simulated_delay_(network_simulated_delay) {
    std::random_device seed;
    urng_ = std::mt19937_64(seed());
  }
  virtual ~TaraxaCapability() = default;
  std::string name() const override { return "taraxa"; }
  unsigned version() const override { return 1; }
  unsigned messageCount() const override { return PacketCount; }

  void onStarting() override;
  void onStopping() override {
    if (network_simulated_delay_ > 0) io_service_.stop();
  }

  void onConnect(NodeID const &_nodeID, u256 const &) override;
  void syncPeer(NodeID const &_nodeID);
  void continueSync(NodeID const &_nodeID);
  bool interpretCapabilityPacket(NodeID const &_nodeID, unsigned _id,
                                 RLP const &_r) override;
  bool interpretCapabilityPacketImpl(NodeID const &_nodeID, unsigned _id,
                                     RLP const &_r);
  void onDisconnect(NodeID const &_nodeID) override;
  void sendTestMessage(NodeID const &_id, int _x);
  void onNewBlockReceived(DagBlock block,
                          std::vector<Transaction> transactions);
  void onNewBlockVerified(DagBlock block);
  void onNewTransactions(
      std::unordered_map<trx_hash_t, Transaction> const &transactions,
      bool fromNetwork);
  vector<NodeID> selectPeers(
      std::function<bool(TaraxaPeer const &)> const &_predicate);
  vector<NodeID> getAllPeers() const;
  std::pair<std::vector<NodeID>, std::vector<NodeID>> randomPartitionPeers(
      std::vector<NodeID> const &_peers, std::size_t _number);
  std::pair<int, int> retrieveTestData(NodeID const &_id);
  void sendBlock(NodeID const &_id, taraxa::DagBlock block, bool newBlock);
  void sendChildren(NodeID const &_id, std::vector<std::string> children);
  void sendBlockHash(NodeID const &_id, taraxa::DagBlock block);
  void requestBlock(NodeID const &_id, blk_hash_t hash, bool newBlock);
  void requestBlockChildren(NodeID const &_id, std::vector<std::string> leaves);
  void sendTransactions(NodeID const &_id,
                        std::vector<Transaction> const &transactions);

  std::map<blk_hash_t, taraxa::DagBlock> getBlocks();
  std::map<trx_hash_t, taraxa::Transaction> getTransactions();
  void setFullNode(std::shared_ptr<FullNode> full_node);

  void doBackgroundWork();

  // PBFT
  void onNewPbftVote(taraxa::Vote const &vote);
  void sendPbftVote(NodeID const &_id, taraxa::Vote const &vote);
  void onNewPbftBlock(taraxa::PbftBlock const &pbft_block);
  void sendPbftBlock(NodeID const &_id, taraxa::PbftBlock const &pbft_block);

 private:
  const int c_backround_work_period_ms_ = 1000;
  Host &host_;
  std::unordered_map<NodeID, int> cnt_received_messages_;
  std::unordered_map<NodeID, int> test_sums_;

  // Only used for testing without the full node set
  std::map<blk_hash_t, taraxa::DagBlock> test_blocks_;
  std::map<trx_hash_t, Transaction> test_transactions_;

  std::set<blk_hash_t> block_requestes_set_;

  std::weak_ptr<FullNode> full_node_;

  std::unordered_map<NodeID, TaraxaPeer> peers_;
  uint16_t network_simulated_delay_;
  boost::thread_group delay_threads_;
  boost::asio::io_service io_service_;
  std::shared_ptr<boost::asio::io_service::work> io_work_;
  mutable std::mt19937_64
      urng_;  // Mersenne Twister psuedo-random number generator
  dev::Logger logger_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TARCAP")};
  dev::Logger logger_debug_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TARCAP")};
  dev::Logger logger_err_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TARCAP")};
};
}  // namespace taraxa
#endif