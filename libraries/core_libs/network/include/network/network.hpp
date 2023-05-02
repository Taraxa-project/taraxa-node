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
#include "network/tarcap/taraxa_capability_base.hpp"
#include "network/tarcap/tarcap_version.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

namespace network::tarcap {
class TimePeriodPacketsStats;
class NodeStats;
}  // namespace network::tarcap

class PacketHandler;

class Network {
 public:
  Network(const FullNodeConfig &config, const h256 &genesis_hash = {},
          dev::p2p::Host::CapabilitiesFactory construct_capabilities = {},
          std::filesystem::path const &network_file_path = {}, dev::KeyPair const &key = dev::KeyPair::create(),
          std::shared_ptr<DbStorage> db = {}, std::shared_ptr<PbftManager> pbft_mgr = {},
          std::shared_ptr<PbftChain> pbft_chain = {}, std::shared_ptr<VoteManager> vote_mgr = {},
          std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<TransactionManager> trx_mgr = {});

  ~Network();
  Network(const Network &) = delete;
  Network(Network &&) = delete;
  Network &operator=(const Network &) = delete;
  Network &operator=(Network &&) = delete;

  // METHODS USED IN REAL CODE
  void start();
  bool isStarted();
  std::list<dev::p2p::NodeEntry> getAllNodes() const;
  size_t getPeerCount();

  // returns count of all discovered nodes
  unsigned getNodeCount();
  Json::Value getStatus();
  void startSyncingPbft();
  bool pbft_syncing();
  uint64_t syncTimeSeconds() const;
  void setSyncStatePeriod(PbftPeriod period);

  void gossipDagBlock(const DagBlock &block, bool proposed, const SharedTransactions &trxs);
  void gossipVote(const std::shared_ptr<Vote> &vote, const std::shared_ptr<PbftBlock> &block, bool rebroadcast = false);
  void gossipVotesBundle(const std::vector<std::shared_ptr<Vote>> &votes, bool rebroadcast = false);
  void handleMaliciousSyncPeer(const dev::p2p::NodeID &id);
  std::shared_ptr<network::tarcap::TaraxaPeer> getMaxChainPeer() const;

  // METHODS USED IN TESTS ONLY
  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler() const;

  void setPendingPeersToReady();
  dev::p2p::NodeID getNodeId() const;
  int getReceivedBlocksCount() const;
  int getReceivedTransactionsCount() const;
  std::shared_ptr<network::tarcap::TaraxaPeer> getPeer(dev::p2p::NodeID const &id) const;
  // END METHODS USED IN TESTS ONLY

 private:
  static std::pair<bool, bi::tcp::endpoint> resolveHost(string const &addr, uint16_t port);

  /**
   * @brief Register period events, e.g. sending status packet, transaction packet etc...
   *
   * @param config
   * @param pbft_mgr
   * @param trx_mgr
   */
  void registerPeriodicEvents(const std::shared_ptr<PbftManager> &pbft_mgr,
                              std::shared_ptr<TransactionManager> trx_mgr);

  void addBootNodes(bool initial = false);

 private:
  // Node config
  const FullNodeConfig &kConf;

  // Node public key
  const dev::Public pub_key_;

  // Packets stats per time period
  // TODO: maybe remove tarcap namespace ???
  std::shared_ptr<network::tarcap::TimePeriodPacketsStats> all_packets_stats_;

  // Node stats
  // TODO: maybe remove tarcap namespace ???
  std::shared_ptr<network::tarcap::NodeStats> node_stats_;

  // Syncing state
  // TODO: maybe remove tarcap namespace ???
  std::shared_ptr<network::tarcap::PbftSyncingState> pbft_syncing_state_;

  util::ThreadPool tp_;
  std::shared_ptr<dev::p2p::Host> host_;

  // All supported taraxa capabilities
  std::map<network::tarcap::TarcapVersion, std::shared_ptr<network::tarcap::TaraxaCapabilityBase>> tarcaps_;

  // Threadpool for packets
  std::shared_ptr<network::threadpool::PacketsThreadPool> packets_tp_;

  // Threadpool for periodic and delayed events
  // TODO: tp_ could be used for this instead
  util::ThreadPool periodic_events_tp_;

  LOG_OBJECTS_DEFINE
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> Network::getSpecificHandler() const {
  return tarcaps_.rbegin()->second->getSpecificHandler<PacketHandlerType>();
}

}  // namespace taraxa
