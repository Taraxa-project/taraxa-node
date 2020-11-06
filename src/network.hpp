#ifndef TARAXA_NODE_NETWORK_HPP
#define TARAXA_NODE_NETWORK_HPP
#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>

#include <atomic>
#include <boost/thread.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

#include "config.hpp"
#include "dag_block.hpp"
#include "taraxa_capability.hpp"
#include "transaction.hpp"
#include "util.hpp"
#include "vote.hpp"

namespace taraxa {

/**
 */

class Network {
 public:
  Network(NetworkConfig const &config, std::string const &genesis, addr_t node_addr);
  Network(NetworkConfig const &config, std::string const &networkFile, std::string const &genesis, addr_t node_addr, std::shared_ptr<DbStorage> db,
          std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
          std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<BlockManager> blk_mgr, std::shared_ptr<TransactionManager> trx_mgr, public_t node_pk,
          uint32_t lambda_ms_min);
  Network(NetworkConfig const &config, std::string const &networkFile, secret_t const &sk, std::string const &genesis, addr_t node_addr,
          std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
          std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<BlockManager> blk_mgr,
          std::shared_ptr<TransactionManager> trx_mgr, public_t node_pk, uint32_t lambda_ms_min);
  ~Network();
  void start(bool boot_node = false);
  void stop();
  bool isStarted();
  bool isSynced() { return taraxa_capability_ && !taraxa_capability_->syncing_; }
  void rpcAction(boost::system::error_code const &ec, size_t size);
  void sendTest(dev::p2p::NodeID const &id);
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk, bool newBlock);
  void sendTransactions(NodeID const &_id, std::vector<taraxa::bytes> const &transactions);
  void onNewBlockVerified(DagBlock const &blk);
  void onNewTransactions(std::vector<taraxa::bytes> const &transactions);
  NetworkConfig getConfig();
  // no need to set full node in network testing
  void saveNetwork(std::string fileName);
  int getPeerCount() { return taraxa_capability_->getPeersCount(); }
  int getNodeCount() { return host_->getNodeCount(); }
  std::list<NodeEntry> getAllNodes() const { return host_->getNodes(); }
  std::vector<NodeID> getAllPeers() const { return taraxa_capability_->getAllPeers(); }
  dev::p2p::NodeID getNodeId() { return host_->id(); };
  int getReceivedBlocksCount() { return taraxa_capability_->getBlocks().size(); }
  int getReceivedTransactionsCount() { return taraxa_capability_->getTransactions().size(); }
  std::shared_ptr<TaraxaCapability> getTaraxaCapability() const { return taraxa_capability_; }

  // PBFT
  void onNewPbftVote(Vote const &vote);
  void sendPbftVote(NodeID const &id, Vote const &vote);
  void onNewPbftBlock(PbftBlock const &pbft_block);
  void sendPbftBlock(NodeID const &id, PbftBlock const &pbft_block, uint64_t const &pbft_chain_size);

  std::pair<bool, bi::tcp::endpoint> resolveHost(string const &addr, uint16_t port);

 private:
  std::shared_ptr<dev::p2p::Host> host_;
  std::shared_ptr<TaraxaCapability> taraxa_capability_;

  NetworkConfig conf_;
  std::atomic<bool> stopped_ = true;
  std::string network_file_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<BlockManager> blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  public_t node_pk_;
  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa
#endif
