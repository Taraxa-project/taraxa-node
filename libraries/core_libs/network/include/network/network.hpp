#pragma once

#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>

#include <boost/thread.hpp>

#include "common/thread_pool.hpp"
#include "config/config.hpp"
#include "network/tarcap/taraxa_capability.hpp"
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
  Network(const FullNodeConfig &config, const h256 &genesis_hash, const std::filesystem::path &network_file_path,
          std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
          std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
          std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<SlashingManager> slashing_manager,
          std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_mgr,
          std::shared_ptr<final_chain::FinalChain> final_chain);

  ~Network();
  Network(const Network &) = delete;
  Network(Network &&) = delete;
  Network &operator=(const Network &) = delete;
  Network &operator=(Network &&) = delete;

  /**
   * @brief Starts threadpools for packets communication in general, specific packets processing and periodic events
   */
  void start();

  bool isStarted();
  std::list<dev::p2p::NodeEntry> getAllNodes() const;
  size_t getPeerCount();

  // returns count of all discovered nodes
  unsigned getNodeCount();
  Json::Value getStatus();
  bool pbft_syncing();
  uint64_t syncTimeSeconds() const;
  void setSyncStatePeriod(PbftPeriod period);

  void gossipDagBlock(const std::shared_ptr<DagBlock> &block, bool proposed, const SharedTransactions &trxs);
  void gossipVote(const std::shared_ptr<PbftVote> &vote, const std::shared_ptr<PbftBlock> &block,
                  bool rebroadcast = false);
  void gossipVotesBundle(const std::vector<std::shared_ptr<PbftVote>> &votes, bool rebroadcast = false);
  void gossipPillarBlockVote(const std::shared_ptr<PillarVote> &vote, bool rebroadcast = false);
  void handleMaliciousSyncPeer(const dev::p2p::NodeID &id);
  std::shared_ptr<network::tarcap::TaraxaPeer> getMaxChainPeer() const;

  /**
   * @brief Request pillar block votes bundle packet from random peer
   *
   * @param period
   * @param pillar_block_hash
   */
  void requestPillarBlockVotesBundle(PbftPeriod period, const blk_hash_t &pillar_block_hash);

  /**
   * @brief Get packets queue status
   *
   * @return true if packets queue is over the limit
   */
  bool packetQueueOverLimit() const;

  // METHODS USED IN TESTS ONLY
  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler(network::SubprotocolPacketType packet_type) const;

  dev::p2p::NodeID getNodeId() const;
  std::shared_ptr<network::tarcap::TaraxaPeer> getPeer(dev::p2p::NodeID const &id) const;
  // END METHODS USED IN TESTS ONLY

 private:
  static std::pair<bool, bi::tcp::endpoint> resolveHost(std::string const &addr, uint16_t port);

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

  // Packets stats per time period
  std::shared_ptr<network::tarcap::TimePeriodPacketsStats> all_packets_stats_;

  // Node stats
  std::shared_ptr<network::tarcap::NodeStats> node_stats_;

  // Syncing state
  std::shared_ptr<network::tarcap::PbftSyncingState> pbft_syncing_state_;

  // Pbft manager
  std::shared_ptr<PbftManager> pbft_mgr_;

  util::ThreadPool tp_;
  std::shared_ptr<dev::p2p::Host> host_;

  // All supported taraxa capabilities - in descending order
  std::map<network::tarcap::TarcapVersion, std::shared_ptr<network::tarcap::TaraxaCapability>,
           std::greater<network::tarcap::TarcapVersion>>
      tarcaps_;

  // Threadpool for packets
  std::shared_ptr<network::threadpool::PacketsThreadPool> packets_tp_;

  // Threadpool for periodic and delayed events
  util::ThreadPool periodic_events_tp_;

  LOG_OBJECTS_DEFINE
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> Network::getSpecificHandler(network::SubprotocolPacketType packet_type) const {
  return tarcaps_.begin()->second->getSpecificHandler<PacketHandlerType>(packet_type);
}

}  // namespace taraxa
