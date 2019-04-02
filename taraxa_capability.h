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

  std::map<blk_hash_t, DagBlock> m_syncBlocks;
  blk_hash_t m_lastRequest;
  PeerState m_state;

 private:
  std::set<blk_hash_t> m_knownBlocks;
  std::set<trx_hash_t> m_knownTransactions;
  NodeID m_id;
};

class TaraxaCapability : public CapabilityFace, public Worker {
 public:
  TaraxaCapability(Host const &_host) : Worker("taraxa"), m_host(_host) {
    std::random_device seed;
    m_urng = std::mt19937_64(seed());
  }
  virtual ~TaraxaCapability() = default;
  std::string name() const override { return "taraxa"; }
  unsigned version() const override { return 1; }
  unsigned messageCount() const override { return PacketCount; }

  void onStarting() override {}
  void onStopping() override {}

  void onConnect(NodeID const &_nodeID, u256 const &) override;
  void syncPeer(NodeID const &_nodeID);
  void continueSync(NodeID const &_nodeID);
  bool interpretCapabilityPacket(NodeID const &_nodeID, unsigned _id,
                                 RLP const &_r) override;
  void onDisconnect(NodeID const &_nodeID) override;
  void sendTestMessage(NodeID const &_id, int _x);
  void onNewBlock(DagBlock block);
  void onNewTransactions(std::vector<Transaction> const &transactions);
  vector<NodeID> selectPeers(
      std::function<bool(TaraxaPeer const &)> const &_predicate);
  std::pair<std::vector<NodeID>, std::vector<NodeID>> randomPartitionPeers(
      std::vector<NodeID> const &_peers, std::size_t _number);
  std::pair<int, int> retrieveTestData(NodeID const &_id);
  void sendBlock(NodeID const &_id, taraxa::DagBlock block, bool newBlock);
  void sendChildren(NodeID const &_id, std::vector<std::string> children);
  void sendBlockHash(NodeID const &_id, taraxa::DagBlock block);
  void requestBlock(NodeID const &_id, blk_hash_t hash, bool newBlock);
  void requestBlockChildren(NodeID const &_id, std::vector<std::string> leaves);
  void sendTransactions(NodeID const &_id, std::vector<Transaction> transactions);

  std::map<blk_hash_t, taraxa::DagBlock> getBlocks();
  std::map<trx_hash_t, taraxa::Transaction> getTransactions();
  void setFullNode(std::shared_ptr<FullNode> full_node);

  Host const &m_host;
  std::unordered_map<NodeID, int> m_cntReceivedMessages;
  std::unordered_map<NodeID, int> m_testSums;

  // Only used for testing without the full node set
  std::map<blk_hash_t, taraxa::DagBlock> m_TestBlocks;
  std::map<trx_hash_t, Transaction> m_TestTransactions;

  std::set<blk_hash_t> m_blockRequestedSet;

  std::weak_ptr<FullNode> full_node_;

  std::unordered_map<NodeID, TaraxaPeer> m_peers;
  mutable std::mt19937_64
      m_urng;  // Mersenne Twister psuedo-random number generator
  dev::Logger logger_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "network")};
  dev::Logger logger_debug_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "network")};
};
}  // namespace taraxa
#endif