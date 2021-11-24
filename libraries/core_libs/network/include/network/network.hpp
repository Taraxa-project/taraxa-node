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

#include "common/thread_pool.hpp"
#include "common/util.hpp"
#include "config/config.hpp"
#include "dag/dag_block_manager.hpp"
#include "tarcap/taraxa_capability.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

struct TaraxaCapability;
class TaraxaPeer;

// TODO merge with TaraxaCapability, and then split the result in reasonable components
class Network {
 public:
  Network(NetworkConfig const &config, std::filesystem::path const &network_file_path = {},
          dev::KeyPair const &key = dev::KeyPair::create(), std::shared_ptr<DbStorage> db = {},
          std::shared_ptr<PbftManager> pbft_mgr = {}, std::shared_ptr<PbftChain> pbft_chain = {},
          std::shared_ptr<VoteManager> vote_mgr = {}, std::shared_ptr<NextVotesManager> next_votes_mgr = {},
          std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<DagBlockManager> dag_blk_mgr = {},
          std::shared_ptr<TransactionManager> trx_mgr = {});

  ~Network();

  static std::pair<bool, bi::tcp::endpoint> resolveHost(string const &addr, uint16_t port);

  // METHODS USED IN REAL CODE
  void start();
  bool isStarted();
  std::list<dev::p2p::NodeEntry> getAllNodes() const;
  size_t getPeerCount();
  unsigned getNodeCount();
  Json::Value getStatus();
  Json::Value getPacketsStats();
  std::vector<dev::p2p::NodeID> getAllPeersIDs() const;
  void onNewBlockVerified(DagBlock const &blk, bool proposed);
  void onNewTransactions(SharedTransactions &&transactions);
  void restartSyncingPbft(bool force = false);
  void onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block);
  bool pbft_syncing();
  uint64_t syncTimeSeconds() const;

  void handleMaliciousSyncPeer(dev::p2p::NodeID const &id);

  void onNewPbftVotes(std::vector<std::shared_ptr<Vote>> votes);
  void broadcastPreviousRoundNextVotesBundle();

  // METHODS USED IN TESTS ONLY
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk);
  void sendBlocks(dev::p2p::NodeID const &id, std::vector<std::shared_ptr<DagBlock>> blocks);
  void sendTransactions(dev::p2p::NodeID const &_id, std::vector<taraxa::bytes> const &transactions);
  void setPendingPeersToReady();
  dev::p2p::NodeID getNodeId() const;
  int getReceivedBlocksCount() const;
  int getReceivedTransactionsCount() const;
  std::shared_ptr<network::tarcap::TaraxaPeer> getPeer(dev::p2p::NodeID const &id) const;

  // PBFT
  void sendPbftBlock(dev::p2p::NodeID const &id, PbftBlock const &pbft_block, uint64_t const &pbft_chain_size);
  void sendPbftVote(dev::p2p::NodeID const &id, std::shared_ptr<Vote> const &vote);
  // END METHODS USED IN TESTS ONLY

 private:
  NetworkConfig conf_;
  util::ThreadPool tp_;
  std::shared_ptr<dev::p2p::Host> host_;
  std::shared_ptr<network::tarcap::TaraxaCapability> taraxa_capability_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
