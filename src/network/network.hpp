#pragma once

#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
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

#include "config/config.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block_manager.hpp"
#include "taraxa_capability.hpp"
#include "transaction_manager/transaction.hpp"
#include "util/thread_pool.hpp"
#include "util/util.hpp"

namespace taraxa {

// TODO merge with TaraxaCapability, and then split the result in reasonable components
class Network {
 public:
  Network(NetworkConfig const &config, std::filesystem::path const &network_file_path = {},
          dev::KeyPair const &key = dev::KeyPair::create(), std::shared_ptr<DbStorage> db = {},
          std::shared_ptr<PbftManager> pbft_mgr = {}, std::shared_ptr<PbftChain> pbft_chain = {},
          std::shared_ptr<VoteManager> vote_mgr = {}, std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr = {},
          std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<DagBlockManager> dag_blk_mgr = {},
          std::shared_ptr<TransactionManager> trx_mgr = {});

  ~Network();

  static std::pair<bool, bi::tcp::endpoint> resolveHost(string const &addr, uint16_t port);

  // METHODS USED IN REAL CODE
  void start();
  bool isStarted();
  std::list<NodeEntry> getAllNodes() const;
  unsigned getPeerCount();
  unsigned getNodeCount();
  Json::Value getStatus();
  std::vector<NodeID> getAllPeers() const;
  void onNewBlockVerified(shared_ptr<DagBlock> const &blk);
  void onNewTransactions(std::vector<taraxa::bytes> transactions);
  void restartSyncingPbft(bool force = false);
  void onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block);
  bool pbft_syncing();
  uint64_t syncTimeSeconds() const;

  void onNewPbftVotes(std::vector<Vote> votes);
  void broadcastPreviousRoundNextVotesBundle();

  // METHODS USED IN TESTS ONLY
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk);
  void sendBlocks(dev::p2p::NodeID const &id, std::vector<std::shared_ptr<DagBlock>> blocks);
  void sendTransactions(NodeID const &_id, std::vector<taraxa::bytes> const &transactions);
  dev::p2p::NodeID getNodeId();
  int getReceivedBlocksCount();
  int getReceivedTransactionsCount();
  std::shared_ptr<TaraxaPeer> getPeer(NodeID const &id);
  // PBFT
  void sendPbftBlock(NodeID const &id, PbftBlock const &pbft_block, uint64_t const &pbft_chain_size);
  void sendPbftVote(NodeID const &id, Vote const &vote);
  // END METHODS USED IN TESTS ONLY

 private:
  NetworkConfig conf_;
  util::ThreadPool tp_;
  std::shared_ptr<dev::p2p::Host> host_;
  std::shared_ptr<TaraxaCapability> taraxa_capability_;
  std::map<Public, NodeIPEndpoint> boot_nodes_;
  util::ThreadPool diagnostic_thread_{1, false};

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
