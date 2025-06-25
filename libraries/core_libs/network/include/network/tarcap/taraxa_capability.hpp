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
#include "slashing_manager/slashing_manager.hpp"

namespace taraxa {
class DbStorage;
class PbftManager;
class PbftChain;
class VoteManager;
class DagManager;
class TransactionManager;
class SlashingManager;
enum class TransactionStatus;

namespace pillar_chain {
class PillarChainManager;
}

}  // namespace taraxa

namespace taraxa::network::tarcap {

class ISyncPacketHandler;
class IVotePacketHandler;
class IPillarVotePacketHandler;
class IGetPillarVotesBundlePacketHandler;
class ITransactionPacketHandler;
class IDagBlockPacketHandler;

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
      const std::shared_ptr<TransactionManager> &trx_mgr, const std::shared_ptr<SlashingManager> &slashing_manager,
      const std::shared_ptr<pillar_chain::PillarChainManager> &pillar_chain_mgr, TarcapVersion version)>;

  /**
   * @brief Default InitPacketsHandlers function definition with the latest version of packets handlers
   */
  static const InitPacketsHandlers kInitLatestVersionHandlers;
  static const InitPacketsHandlers kInitV5VersionHandlers;

 public:
  TaraxaCapability(TarcapVersion version, const FullNodeConfig &conf, const h256 &genesis_hash,
                   std::weak_ptr<dev::p2p::Host> host,
                   std::shared_ptr<network::threadpool::PacketsThreadPool> threadpool,
                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                   std::shared_ptr<PbftSyncingState> syncing_state, std::shared_ptr<DbStorage> db,
                   std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                   std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<SlashingManager> slashing_manager,
                   std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_mgr,
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

  /**
   * @brief templated getSpecificHandler method for getting specific packet handler based on packet_type
   *
   * @tparam PacketHandlerType
   *
   * @return std::shared_ptr<PacketHandlerType>
   */
  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler(SubprotocolPacketType packet_type) const;

 private:
  bool filterSyncIrrelevantPackets(SubprotocolPacketType packet_type) const;
  void handlePacketQueueOverLimit(std::shared_ptr<dev::p2p::Host> host, dev::p2p::NodeID node_id, size_t tp_queue_size);

 private:
  // Capability version
  TarcapVersion version_;

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

  // Last disconnect time and number of peers
  std::chrono::system_clock::time_point last_ddos_disconnect_time_ = {};
  std::chrono::system_clock::time_point queue_over_limit_start_time_ = {};
  bool queue_over_limit_ = false;
  uint32_t last_disconnect_number_of_peers_ = 0;

  logger::Logger logger_;
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> TaraxaCapability::getSpecificHandler(SubprotocolPacketType packet_type) const {
  // Note: Allow to manually cast only to known base classes types.
  // We support multiple taraxa capabilities, which can contain different versions of packet handlers and casting
  // directly to final classes types breaks the functionality...
  switch (packet_type) {
    case SubprotocolPacketType::kPbftSyncPacket:
    case SubprotocolPacketType::kStatusPacket:
      if (!std::is_same<ISyncPacketHandler, PacketHandlerType>::value) {
        assert(false);
      }
      break;

    case SubprotocolPacketType::kTransactionPacket:
      if (!std::is_same<ITransactionPacketHandler, PacketHandlerType>::value) {
        assert(false);
      }
      break;

    case SubprotocolPacketType::kVotePacket:
    case SubprotocolPacketType::kVotesBundlePacket:
      if (!std::is_same<IVotePacketHandler, PacketHandlerType>::value) {
        assert(false);
      }
      break;

    case SubprotocolPacketType::kPillarVotePacket:
      if (!std::is_same<IPillarVotePacketHandler, PacketHandlerType>::value) {
        assert(false);
      }
      break;

    case SubprotocolPacketType::kGetPillarVotesBundlePacket:
      if (!std::is_same<IGetPillarVotesBundlePacketHandler, PacketHandlerType>::value) {
        assert(false);
      }
      break;

    case SubprotocolPacketType::kDagBlockPacket:
      if (!std::is_same<IDagBlockPacketHandler, PacketHandlerType>::value) {
        assert(false);
      }
      break;

    default:
      assert(false);
      return nullptr;
  }

  auto handler = packets_handlers_->getSpecificHandler(packet_type);
  return std::dynamic_pointer_cast<PacketHandlerType>(handler);
}

}  // namespace taraxa::network::tarcap
