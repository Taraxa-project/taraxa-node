#pragma once

#include <libp2p/Capability.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>

//#include <chrono>
#include <memory>
//#include <set>
//#include <thread>

#include "config/config.hpp"
//#include "consensus/vote.hpp"
//#include "dag/dag_block_manager.hpp"
//#include "packet_types.hpp"
#include "threadpool/tarcap_thread_pool.hpp"
//#include "transaction_manager/transaction.hpp"
//#include "util/thread_pool.hpp"
//#include "util/util.hpp"
#include "util/thread_pool.hpp"

namespace taraxa {
class PbftManager;
class PbftChain;
class VoteManager;
class NextVotesForPreviousRound;
class DagManager;
class DagBlockManager;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PeersState;
class SyncingState;
class PacketsHandler;

// TODO: why virtual inheritance ?
class TaraxaCapability : virtual dev::p2p::CapabilityFace {
 public:
  TaraxaCapability(std::weak_ptr<dev::p2p::Host> _host, NetworkConfig const &conf, std::shared_ptr<DbStorage> db = {},
                   std::shared_ptr<PbftManager> pbft_mgr = {}, std::shared_ptr<PbftChain> pbft_chain = {},
                   std::shared_ptr<VoteManager> vote_mgr = {},
                   std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr = {},
                   std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<DagBlockManager> dag_blk_mgr = {},
                   std::shared_ptr<TransactionManager> trx_mgr = {}, addr_t const &node_addr = {});

  virtual ~TaraxaCapability() = default;

  // CapabilityFace implemented interface
  std::string name() const override;
  unsigned version() const override;
  unsigned messageCount() const override;
  void onConnect(std::weak_ptr<dev::p2p::Session> session, u256 const &) override;
  void onDisconnect(dev::p2p::NodeID const &_nodeID) override;
  void interpretCapabilityPacket(std::weak_ptr<dev::p2p::Session> session, unsigned _id, RLP const &_r) override;
  std::string packetTypeToString(unsigned _packetType) const override;

  /**
   * @brief Start processing packets
   */
  void start();

  /**
   * @brief Stop processing packets
   */
  void stop();

  // TODO: delete me
  void pushData(unsigned _id, RLP const &_r);

  // TODO: tarcap threadpool might be responsible for verifying, putting objects into internal structures, etc... and in
  //       such case we would not need to broadcast packets from outside of packet handlers (except very few cases)
  //       and most of these methods could be deleted
  // Interface required in network class to access packets handlers functionality
  //  // METHODS USED IN REAL CODE
  //  void start();
  //  bool isStarted();
  //  std::list<dev::p2p::NodeEntry> getAllNodes() const;
  //  unsigned getPeerCount();
  //  unsigned getNodeCount();
  //  Json::Value getStatus();
  //  std::vector<dev::p2p::NodeID> getAllPeers() const;
  //  void onNewBlockVerified(std::shared_ptr<DagBlock> const &blk);
  //  void onNewTransactions(std::vector<taraxa::bytes> transactions);
  //  void restartSyncingPbft(bool force = false);
  //  void onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block);
  //  bool pbft_syncing();
  //  void onNewPbftVotes(std::vector<Vote> votes);
  //  void broadcastPreviousRoundNextVotesBundle();
  //  void sendTransactions(dev::p2p::NodeID const &_id, std::vector<taraxa::bytes> const &transactions);
  //
  //  // METHODS USED IN TESTS ONLY
  //  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk);
  //  void sendBlocks(dev::p2p::NodeID const &id, std::vector<std::shared_ptr<DagBlock>> blocks);
  //  void setPendingPeersToReady();
  //  dev::p2p::NodeID getNodeId();
  //  int getReceivedBlocksCount();
  //  int getReceivedTransactionsCount();
  //  std::shared_ptr<TaraxaPeer> getPeer(dev::p2p::NodeID const &id);
  //
  //  // PBFT
  //  void sendPbftBlock(dev::p2p::NodeID const &id, PbftBlock const &pbft_block, uint64_t const &pbft_chain_size);
  //  void sendPbftVote(dev::p2p::NodeID const &id, Vote const &vote);
  //  // END METHODS USED IN TESTS ONLY

 private:
  // Peers state
  std::shared_ptr<PeersState> peers_state_;

  // Syncing state
  std::shared_ptr<SyncingState> syncing_state_;

  // Packets handlers
  std::shared_ptr<PacketsHandler> packets_handlers_;

  // Main Threadpool for processing packets
  TarcapThreadPool thread_pool_;

  // TODO: refactor this: we could have some shared global threadpool for periodic events ?
  // Fake threadpool (1 thread) for periodic events like printing summary logs, packets stats, etc...
  util::ThreadPool periodic_events_tp_;

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa::network::tarcap
