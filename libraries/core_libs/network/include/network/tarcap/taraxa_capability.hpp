#pragma once

#include <libp2p/Capability.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>

#include <memory>

#include "common/thread_pool.hpp"
#include "config/config.hpp"
#include "network/tarcap/packets_handler.hpp"
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
class DagManager;
class TransactionManager;
enum class TransactionStatus;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PacketsHandler;
class PbftSyncingState;
class TaraxaPeer;

class TaraxaCapability : public dev::p2p::CapabilityFace {
 public:
  TaraxaCapability(std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key, const FullNodeConfig &conf,
                   unsigned version, const std::string &log_channel);

  virtual ~TaraxaCapability() = default;
  TaraxaCapability(const TaraxaCapability &ro) = delete;
  TaraxaCapability &operator=(const TaraxaCapability &ro) = delete;
  TaraxaCapability(TaraxaCapability &&ro) = delete;
  TaraxaCapability &operator=(TaraxaCapability &&ro) = delete;
  static std::shared_ptr<TaraxaCapability> make(
      std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key, const FullNodeConfig &conf, const h256 &genesis_hash,
      unsigned version, std::shared_ptr<DbStorage> db = {}, std::shared_ptr<PbftManager> pbft_mgr = {},
      std::shared_ptr<PbftChain> pbft_chain = {}, std::shared_ptr<VoteManager> vote_mgr = {},
      std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<TransactionManager> trx_mgr = {},
      const std::string &log_channel = "TARCAP");
  // Init capability. Register packet handlers and periodic events
  virtual void init(const h256 &genesis_hash, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr,
                    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                    std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                    const dev::Address &node_addr);
  // CapabilityFace implemented interface
  std::string name() const override;
  unsigned version() const override;
  unsigned messageCount() const override;
  void onConnect(std::weak_ptr<dev::p2p::Session> session, u256 const &) override;
  void onDisconnect(dev::p2p::NodeID const &_nodeID) override;
  void interpretCapabilityPacket(std::weak_ptr<dev::p2p::Session> session, unsigned _id, dev::RLP const &_r) override;
  std::string packetTypeToString(unsigned _packetType) const override;

  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler() const;

  /**
   * @brief Start processing packets
   */
  void start();

  /**
   * @brief Stop processing packets
   */
  void stop();

  // Interface required in network class to access packets handlers functionality
  // METHODS USED IN REAL CODE
  const std::shared_ptr<PeersState> &getPeersState();
  const std::shared_ptr<NodeStats> &getNodeStats();

  bool pbft_syncing() const;
  void setSyncStatePeriod(PbftPeriod period);

  // METHODS USED IN TESTS ONLY
  size_t getReceivedBlocksCount() const;
  size_t getReceivedTransactionsCount() const;
  // END METHODS USED IN TESTS ONLY

 protected:
  virtual void initPeriodicEvents(const std::shared_ptr<PbftManager> &pbft_mgr,
                                  std::shared_ptr<TransactionManager> trx_mgr,
                                  std::shared_ptr<TimePeriodPacketsStats> packets_stats);
  virtual void registerPacketHandlers(const h256 &genesis_hash,
                                      const std::shared_ptr<TimePeriodPacketsStats> &packets_stats,
                                      const std::shared_ptr<DbStorage> &db,
                                      const std::shared_ptr<PbftManager> &pbft_mgr,
                                      const std::shared_ptr<PbftChain> &pbft_chain,
                                      const std::shared_ptr<VoteManager> &vote_mgr,
                                      const std::shared_ptr<DagManager> &dag_mgr,
                                      const std::shared_ptr<TransactionManager> &trx_mgr, const addr_t &node_addr);

 private:
  void addBootNodes(bool initial = false);

  bool filterSyncIrrelevantPackets(SubprotocolPacketType packet_type) const;

 public:
  // TODO: Remove in future when tests are refactored
  // Test state
  std::shared_ptr<TestState> test_state_;

 protected:
  // Capability version
  unsigned version_;

  // Packets stats per time period
  std::shared_ptr<TimePeriodPacketsStats> all_packets_stats_;

  // Node config
  const FullNodeConfig &kConf;

  // Peers state
  std::shared_ptr<PeersState> peers_state_;

  // Syncing state + syncing handler
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;

  // Node stats
  std::shared_ptr<NodeStats> node_stats_;

  // Packets handlers
  std::shared_ptr<PacketsHandler> packets_handlers_;

  // Main Threadpool for processing packets
  std::shared_ptr<TarcapThreadPool> thread_pool_;

  // Threadpool for periodic and delayed events
  std::shared_ptr<util::ThreadPool> periodic_events_tp_;

  // Node public key
  const dev::Public pub_key_;

  LOG_OBJECTS_DEFINE
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> TaraxaCapability::getSpecificHandler() const {
  return packets_handlers_->getSpecificHandler<PacketHandlerType>();
}

}  // namespace taraxa::network::tarcap
