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
#include "network/tarcap/tarcap_version.hpp"
#include "network/threadpool/tarcap_thread_pool.hpp"
#include "pbft/pbft_chain.hpp"

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

class TaraxaCapability final : public dev::p2p::CapabilityFace {
 public:
  /**
   * @brief Function signature for creating taraxa capability packets handlers
   */
  using InitPacketsHandlers = std::function<std::shared_ptr<PacketsHandler>(
      const std::string &logs_prefix, const FullNodeConfig &config, const h256 &genesis_hash,
      const std::shared_ptr<PeersState> &peers_state, const std::shared_ptr<PbftSyncingState> &pbft_syncing_state,

      const std::shared_ptr<tarcap::TimePeriodPacketsStats> &packets_stats, const std::shared_ptr<DbStorage> &db,
      const std::shared_ptr<PbftManager> &pbft_mgr, const std::shared_ptr<PbftChain> &pbft_chain,
      const std::shared_ptr<VoteManager> &vote_mgr, const std::shared_ptr<DagManager> &dag_mgr,
      const std::shared_ptr<TransactionManager> &trx_mgr, const addr_t &node_addr)>;

  /**
   * @brief Default InitPacketsHandlers function definition with the latest version of packets handlers
   */
  static const InitPacketsHandlers kInitLatestVersionHandlers;

 public:
  TaraxaCapability(TarcapVersion version, const FullNodeConfig &conf, const h256 &genesis_hash,
                   std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key,
                   std::shared_ptr<network::threadpool::PacketsThreadPool> threadpool,
                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                   std::shared_ptr<PbftSyncingState> syncing_state, std::shared_ptr<DbStorage> db,
                   std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                   std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr,
                   InitPacketsHandlers init_packets_handlers = kInitLatestVersionHandlers);

  virtual ~TaraxaCapability() = default;
  TaraxaCapability(const TaraxaCapability &ro) = delete;
  TaraxaCapability &operator=(const TaraxaCapability &ro) = delete;
  TaraxaCapability(TaraxaCapability &&ro) = delete;
  TaraxaCapability &operator=(TaraxaCapability &&ro) = delete;

  // CapabilityFace implemented interface
  std::string name() const override;
  TarcapVersion version() const override;
  unsigned messageCount() const override;
  void onConnect(std::weak_ptr<dev::p2p::Session> session, u256 const &) override;
  void onDisconnect(dev::p2p::NodeID const &_nodeID) override;
  void interpretCapabilityPacket(std::weak_ptr<dev::p2p::Session> session, unsigned _id, dev::RLP const &_r) override;
  std::string packetTypeToString(unsigned _packetType) const override;

  const std::shared_ptr<PeersState> &getPeersState();

  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler() const;

 private:
  bool filterSyncIrrelevantPackets(SubprotocolPacketType packet_type) const;

 private:
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

  // Packets handlers
  std::shared_ptr<PacketsHandler> packets_handlers_;

  // Main Threadpool for processing packets
  std::shared_ptr<threadpool::PacketsThreadPool> thread_pool_;

  LOG_OBJECTS_DEFINE
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> TaraxaCapability::getSpecificHandler() const {
  return packets_handlers_->getSpecificHandler<PacketHandlerType>();
}

}  // namespace taraxa::network::tarcap
