#pragma once

#include <libp2p/Capability.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>

#include <memory>

#include "common/thread_pool.hpp"
#include "config/config.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "network/tarcap/stats/node_stats.hpp"
#include "pbft/pbft_chain.hpp"
#include "threadpool/tarcap_thread_pool.hpp"

namespace taraxa {
class DbStorage;
class PbftManager;
class PbftChain;
class VoteManager;
class NextVotesManager;
class DagManager;
class DagBlockManager;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::final_chain {
class FinalChain;
}

namespace taraxa::network::tarcap {

class PacketsHandler;
class SyncingState;
class TaraxaPeer;

class TaraxaCapability : public dev::p2p::CapabilityFace {
 public:
  TaraxaCapability(std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key, const NetworkConfig &conf,
                   std::shared_ptr<DbStorage> db = {}, std::shared_ptr<PbftManager> pbft_mgr = {},
                   std::shared_ptr<PbftChain> pbft_chain = {}, std::shared_ptr<VoteManager> vote_mgr = {},
                   std::shared_ptr<NextVotesManager> next_votes_mgr = {}, std::shared_ptr<DagManager> dag_mgr = {},
                   std::shared_ptr<DagBlockManager> dag_blk_mgr = {}, std::shared_ptr<TransactionManager> trx_mgr = {},
                   addr_t const &node_addr = {});

  virtual ~TaraxaCapability() = default;

  // CapabilityFace implemented interface
  std::string name() const override;
  unsigned version() const override;
  unsigned messageCount() const override;
  void onConnect(std::weak_ptr<dev::p2p::Session> session, u256 const &) override;
  void onDisconnect(dev::p2p::NodeID const &_nodeID) override;
  void interpretCapabilityPacket(std::weak_ptr<dev::p2p::Session> session, unsigned _id, dev::RLP const &_r) override;
  std::string packetTypeToString(unsigned _packetType) const override;

  /**
   * @brief Start processing packets
   */
  void start();

  /**
   * @brief Stop processing packets
   */
  void stop();

  // TODO: tarcap threadpool might be responsible for verifying, putting objects into internal structures, etc... and in
  //       such case we would not need to broadcast packets from outside of packet handlers (except very few cases)
  //       and most of these methods could be deleted
  // Interface required in network class to access packets handlers functionality
  // METHODS USED IN REAL CODE
  const std::shared_ptr<PeersState> &getPeersState();
  const std::shared_ptr<NodeStats> &getNodeStats();

  void restartSyncingPbft(bool force = false);
  bool pbft_syncing() const;
  void onNewBlockVerified(DagBlock const &blk, bool proposed);
  void onNewTransactions(SharedTransactions &&transactions);
  void onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block);
  void onNewPbftVote(const std::shared_ptr<Vote> &vote);
  void broadcastPreviousRoundNextVotesBundle();
  void sendTransactions(dev::p2p::NodeID const &id, std::vector<taraxa::bytes> const &transactions);
  void handleMaliciousSyncPeer(dev::p2p::NodeID const &id);
  void setSyncStatePeriod(uint64_t period);

  // METHODS USED IN TESTS ONLY
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk);
  void sendBlocks(dev::p2p::NodeID const &id, std::vector<std::shared_ptr<DagBlock>> blocks);
  size_t getReceivedBlocksCount() const;
  size_t getReceivedTransactionsCount() const;

  void onNewBlockReceived(DagBlock &&block);

  void sendTestMessage(dev::p2p::NodeID const &id, int x, std::vector<char> const &data);
  std::pair<size_t, uint64_t> retrieveTestData(const dev::p2p::NodeID &node_id);

  // PBFT
  void sendPbftBlock(dev::p2p::NodeID const &id, PbftBlock const &pbft_block, uint64_t pbft_chain_size);
  void sendPbftVote(dev::p2p::NodeID const &id, std::shared_ptr<Vote> const &vote);

  // END METHODS USED IN TESTS ONLY

 private:
  void initBootNodes(const std::vector<NodeConfig> &network_boot_nodes, const dev::KeyPair &key);
  void initPeriodicEvents(const NetworkConfig &conf, const std::shared_ptr<PbftManager> &pbft_mgr,
                          std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<PacketsStats> packets_stats);
  void registerPacketHandlers(const NetworkConfig &conf, const std::shared_ptr<PacketsStats> &packets_stats,
                              const std::shared_ptr<DbStorage> &db, const std::shared_ptr<PbftManager> &pbft_mgr,
                              const std::shared_ptr<PbftChain> &pbft_chain,
                              const std::shared_ptr<VoteManager> &vote_mgr,
                              const std::shared_ptr<NextVotesManager> &next_votes_mgr,
                              const std::shared_ptr<DagManager> &dag_mgr,
                              const std::shared_ptr<DagBlockManager> &dag_blk_mgr,
                              const std::shared_ptr<TransactionManager> &trx_mgr, addr_t const &node_addr);

  bool filterSyncIrrelevantPackets(SubprotocolPacketType packet_type) const;

 public:
  // TODO: Remove in future when tests are refactored
  // Test state
  std::shared_ptr<TestState> test_state_;

 private:
  // Peers state
  std::shared_ptr<PeersState> peers_state_;

  // Syncing state + syncing handler
  std::shared_ptr<SyncingState> syncing_state_;

  // List of boot nodes (from config)
  std::map<dev::Public, dev::p2p::NodeIPEndpoint> boot_nodes_;

  // Node stats
  std::shared_ptr<NodeStats> node_stats_;

  // Packets handlers
  std::shared_ptr<PacketsHandler> packets_handlers_;

  // Main Threadpool for processing packets
  std::shared_ptr<TarcapThreadPool> thread_pool_;

  // TODO: refactor this: we could have some shared global threadpool for periodic events ?
  // Fake threadpool (1 thread) for periodic events like printing summary logs, packets stats, etc...
  util::ThreadPool periodic_events_tp_;

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa::network::tarcap
