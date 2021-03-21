#pragma once

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

  static std::pair<dev::Secret, dev::p2p::ENR> makeENR(dev::KeyPair const &key,
                                                       dev::p2p::NetworkConfig const &net_conf);

  // METHODS USED IN REAL CODE
  void start();

  bool isStarted() { return !stopped_; }

  auto getAllNodes() const { return host_->getNodes(); }

  auto getPeerCount() { return taraxa_capability_->getPeersCount(); }

  auto getNodeCount() { return host_->getNodeCount(); }

  auto getStatus() { return taraxa_capability_->getStatus(); }

  auto getAllPeers() const { return taraxa_capability_->getAllPeers(); }

  void onNewBlockVerified(shared_ptr<DagBlock> const &blk) {
    tp_.post([=] {
      taraxa_capability_->onNewBlockVerified(*blk);
      LOG(log_dg_) << "On new block verified:" << blk->getHash().toString();
    });
  }

  void onNewTransactions(std::vector<taraxa::bytes> transactions) {
    tp_.post([=, transactions = std::move(transactions)] {
      taraxa_capability_->onNewTransactions(transactions, true);
      LOG(log_dg_) << "On new transactions" << transactions.size();
    });
  }

  void restartSyncingPbft(bool force = false) {
    tp_.post([=] { taraxa_capability_->restartSyncingPbft(force); });
  }

  void onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block) {
    tp_.post([=] {
      LOG(log_dg_) << "Network broadcast PBFT block: " << pbft_block->getBlockHash();
      taraxa_capability_->onNewPbftBlock(*pbft_block);
    });
  }

  auto pbft_syncing() { return taraxa_capability_->syncing_.load(); }

  void onNewPbftVotes(std::vector<Vote> votes) {
    tp_.post([=, votes = std::move(votes)] {
      for (auto const &vote : votes) {
        LOG(log_dg_) << "Network broadcast PBFT vote: " << vote.getHash();
        taraxa_capability_->onNewPbftVote(vote);
      }
    });
  }

  void broadcastPreviousRoundNextVotesBundle() {
    tp_.post([=] {
      LOG(log_dg_) << "Network broadcast previous round next votes bundle";
      taraxa_capability_->broadcastPreviousRoundNextVotesBundle();
    });
  }

  // METHODS USED IN TESTS ONLY
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk) {
    taraxa_capability_->sendBlock(id, blk);
    LOG(log_dg_) << "Sent Block:" << blk.getHash().toString();
  }
  void sendTransactions(NodeID const &_id, std::vector<taraxa::bytes> const &transactions) {
    taraxa_capability_->sendTransactions(_id, transactions);
    LOG(log_dg_) << "Sent transactions:" << transactions.size();
  }
  dev::p2p::NodeID getNodeId() { return host_->id(); };
  int getReceivedBlocksCount() { return taraxa_capability_->getBlocks().size(); }
  int getReceivedTransactionsCount() { return taraxa_capability_->getTransactions().size(); }
  auto getPeer(NodeID const &id) { return taraxa_capability_->getPeer(id); }
  // PBFT
  void sendPbftBlock(NodeID const &id, PbftBlock const &pbft_block, uint64_t const &pbft_chain_size) {
    LOG(log_dg_) << "Network send PBFT block: " << pbft_block.getBlockHash() << " to: " << id;
    taraxa_capability_->sendPbftBlock(id, pbft_block, pbft_chain_size);
  }
  void sendPbftVote(NodeID const &id, Vote const &vote) {
    LOG(log_dg_) << "Network sent PBFT vote: " << vote.getHash() << " to: " << id;
    taraxa_capability_->sendPbftVote(id, vote);
  }
  // END METHODS USED IN TESTS ONLY

 private:
  NetworkConfig conf_;
  util::ThreadPool tp_;
  std::filesystem::path network_file_;
  std::shared_ptr<dev::p2p::Host> host_;
  std::shared_ptr<TaraxaCapability> taraxa_capability_;
  std::map<Public, NodeIPEndpoint> boot_nodes_;
  std::atomic<bool> stopped_ = true;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa
