#include "network/tarcap/taraxa_capability.hpp"

#include <algorithm>

#include "network/tarcap/packets_handler.hpp"
#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/get_dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/get_next_votes_bundle_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/get_pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/status_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/votes_bundle_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "node/node.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote/vote.hpp"

namespace taraxa::network::tarcap {

TaraxaCapability::TaraxaCapability(TarcapVersion version, const FullNodeConfig &conf, const h256 &genesis_hash,
                                   std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key,
                                   std::shared_ptr<network::threadpool::PacketsThreadPool> threadpool,
                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                   std::shared_ptr<PbftSyncingState> syncing_state, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                   std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                                   std::shared_ptr<TransactionManager> trx_mgr,
                                   InitPacketsHandlers init_packets_handlers)
    : version_(version),
      all_packets_stats_(std::move(packets_stats)),
      kConf(conf),
      peers_state_(nullptr),
      pbft_syncing_state_(std::move(syncing_state)),
      packets_handlers_(std::make_shared<PacketsHandler>()),
      thread_pool_(std::move(threadpool)) {
  const std::string logs_prefix = "V" + std::to_string(version) + "_";
  const auto &node_addr = key.address();

  LOG_OBJECTS_CREATE(logs_prefix + "TARCAP");

  peers_state_ = std::make_shared<PeersState>(host, kConf);
  packets_handlers_ =
      init_packets_handlers(logs_prefix, conf, genesis_hash, peers_state_, pbft_syncing_state_, all_packets_stats_, db,
                            pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr, node_addr);

  // Must be called after init_packets_handlers
  thread_pool_->setPacketsHandlers(version, packets_handlers_);
}

std::string TaraxaCapability::name() const { return TARAXA_CAPABILITY_NAME; }

TarcapVersion TaraxaCapability::version() const { return version_; }

unsigned TaraxaCapability::messageCount() const { return SubprotocolPacketType::PacketCount; }

void TaraxaCapability::onConnect(std::weak_ptr<dev::p2p::Session> session, u256 const &) {
  const auto session_p = session.lock();
  if (!session_p) {
    LOG(log_er_) << "Unable to obtain session ptr !";
    return;
  }

  const auto node_id = session_p->id();

  if (peers_state_->is_peer_malicious(node_id)) {
    session_p->disconnect(dev::p2p::UserReason);
    LOG(log_wr_) << "Node " << node_id << " connection dropped - malicious node";
    return;
  }

  peers_state_->addPendingPeer(node_id);
  LOG(log_nf_) << "Node " << node_id << " connected";

  auto status_packet_handler = packets_handlers_->getSpecificHandler<StatusPacketHandler>();
  status_packet_handler->sendStatus(node_id, true);
}

void TaraxaCapability::onDisconnect(dev::p2p::NodeID const &_nodeID) {
  LOG(log_nf_) << "Node " << _nodeID << " disconnected";
  peers_state_->erasePeer(_nodeID);

  const auto syncing_peer = pbft_syncing_state_->syncingPeer();
  if (pbft_syncing_state_->isPbftSyncing() && syncing_peer && syncing_peer->getId() == _nodeID) {
    pbft_syncing_state_->setPbftSyncing(false);
    if (peers_state_->getPeersCount() > 0) {
      LOG(log_dg_) << "Restart PBFT/DAG syncing due to syncing peer disconnect.";
      packets_handlers_->getSpecificHandler<PbftSyncPacketHandler>()->startSyncingPbft();
    } else {
      LOG(log_dg_) << "Stop PBFT/DAG syncing due to syncing peer disconnect and no other peers available.";
    }
  }
}

std::string TaraxaCapability::packetTypeToString(unsigned _packetType) const {
  return convertPacketTypeToString(static_cast<SubprotocolPacketType>(_packetType));
}

void TaraxaCapability::interpretCapabilityPacket(std::weak_ptr<dev::p2p::Session> session, unsigned _id,
                                                 dev::RLP const &_r) {
  const auto session_p = session.lock();
  if (!session_p) {
    LOG(log_er_) << "Unable to obtain session ptr !";
    return;
  }

  auto node_id = session.lock()->id();

  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "Unable to process packet, host == nullptr";
    return;
  }

  const SubprotocolPacketType packet_type = static_cast<SubprotocolPacketType>(_id);

  // Drop any packet (except StatusPacket) that comes before the connection between nodes is initialized by sending
  // and received initial status packet
  const auto peer = peers_state_->getPacketSenderPeer(node_id, packet_type);
  if (!peer.first) [[unlikely]] {
    LOG(log_wr_) << "Unable to push packet into queue. Reason: " << peer.second;
    host->disconnect(node_id, dev::p2p::UserReason);
    return;
  }

  if (pbft_syncing_state_->isDeepPbftSyncing() && filterSyncIrrelevantPackets(packet_type)) {
    LOG(log_dg_) << "Ignored " << convertPacketTypeToString(packet_type) << " because we are still syncing";
    return;
  }

  const auto [hp_queue_size, mp_queue_size, lp_queue_size] = thread_pool_->getQueueSize();
  const size_t tp_queue_size = hp_queue_size + mp_queue_size + lp_queue_size;

  // Check peer's max allowed packets processing time in case peer_max_packets_queue_size_limit was exceeded
  if (kConf.network.ddos_protection.peer_max_packets_queue_size_limit &&
      tp_queue_size > kConf.network.ddos_protection.peer_max_packets_queue_size_limit) {
    const auto [start_time, peer_packets_stats] = peer.first->getAllPacketsStatsCopy();
    // As start_time is reset in independent thread, it might be few ms out of sync - subtract extra 250ms for this
    const auto current_time_period = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time - std::chrono::milliseconds{250});

    if (current_time_period <= kConf.network.ddos_protection.packets_stats_time_period_ms) {
      // Peer exceeded max allowed processing time for his packets
      if (peer_packets_stats.processing_duration_ > kConf.network.ddos_protection.peer_max_packets_processing_time_us) {
        LOG(log_er_) << "Ignored " << convertPacketTypeToString(packet_type) << " from " << node_id
                     << ". Peer's current packets processing time " << peer_packets_stats.processing_duration_.count()
                     << " us, max allowed processing time "
                     << kConf.network.ddos_protection.peer_max_packets_processing_time_us.count()
                     << " us. Peer will be disconnected";
        host->disconnect(node_id, dev::p2p::UserReason);
        return;
      }
    } else {
      LOG(log_wr_) << "Unable to validate peer's max allowed packets processing time due to invalid time period";
    }
  }

  // Check max allowed packets queue size
  if (kConf.network.ddos_protection.max_packets_queue_size &&
      tp_queue_size > kConf.network.ddos_protection.max_packets_queue_size) {
    const auto connected_peers = peers_state_->getAllPeers();
    // Always keep at least 5 connected peers
    if (connected_peers.size() > 5) {
      // Find peer with the highest processing time and disconnect him
      std::pair<std::chrono::microseconds, dev::p2p::NodeID> peer_max_processing_time{std::chrono::microseconds(0),
                                                                                      dev::p2p::NodeID()};

      for (const auto &connected_peer : connected_peers) {
        const auto peer_packets_stats = connected_peer.second->getAllPacketsStatsCopy();

        if (peer_packets_stats.second.processing_duration_ > peer_max_processing_time.first) {
          peer_max_processing_time = {peer_packets_stats.second.processing_duration_, connected_peer.first};
        }
      }

      // Disconnect peer with the highest processing time
      LOG(log_er_) << "Max allowed packets queue size " << kConf.network.ddos_protection.max_packets_queue_size
                   << " exceeded: " << tp_queue_size << ". Peer with the highest processing time "
                   << peer_max_processing_time.second << " will be disconnected";
      host->disconnect(node_id, dev::p2p::UserReason);
      return;
    }
  }

  // TODO: we are making a copy here for each packet bytes(toBytes()), which is pretty significant. Check why RLP does
  //       not support move semantics so we can take advantage of it...
  thread_pool_->push({version(), threadpool::PacketData(packet_type, node_id, _r.data().toBytes())});
}

inline bool TaraxaCapability::filterSyncIrrelevantPackets(SubprotocolPacketType packet_type) const {
  switch (packet_type) {
    case StatusPacket:
    case GetPbftSyncPacket:
    case PbftSyncPacket:
      return false;
    default:
      return true;
  }
}

const std::shared_ptr<PeersState> &TaraxaCapability::getPeersState() { return peers_state_; }

const TaraxaCapability::InitPacketsHandlers TaraxaCapability::kInitLatestVersionHandlers =
    [](const std::string &logs_prefix, const FullNodeConfig &config, const h256 &genesis_hash,
       const std::shared_ptr<PeersState> &peers_state, const std::shared_ptr<PbftSyncingState> &pbft_syncing_state,
       const std::shared_ptr<tarcap::TimePeriodPacketsStats> &packets_stats, const std::shared_ptr<DbStorage> &db,
       const std::shared_ptr<PbftManager> &pbft_mgr, const std::shared_ptr<PbftChain> &pbft_chain,
       const std::shared_ptr<VoteManager> &vote_mgr, const std::shared_ptr<DagManager> &dag_mgr,
       const std::shared_ptr<TransactionManager> &trx_mgr, const addr_t &node_addr) {
      auto packets_handlers = std::make_shared<PacketsHandler>();
      // Consensus packets with high processing priority
      packets_handlers->registerHandler<VotePacketHandler>(config, peers_state, packets_stats, pbft_mgr, pbft_chain,
                                                           vote_mgr, node_addr, logs_prefix);
      packets_handlers->registerHandler<GetNextVotesBundlePacketHandler>(config, peers_state, packets_stats, pbft_mgr,
                                                                         pbft_chain, vote_mgr, node_addr, logs_prefix);
      packets_handlers->registerHandler<VotesBundlePacketHandler>(config, peers_state, packets_stats, pbft_mgr,
                                                                  pbft_chain, vote_mgr, node_addr, logs_prefix);

      // Standard packets with mid processing priority
      packets_handlers->registerHandler<DagBlockPacketHandler>(config, peers_state, packets_stats, pbft_syncing_state,
                                                               pbft_chain, pbft_mgr, dag_mgr, trx_mgr, db, node_addr,
                                                               logs_prefix);

      packets_handlers->registerHandler<TransactionPacketHandler>(config, peers_state, packets_stats, trx_mgr,
                                                                  node_addr, logs_prefix);

      // Non critical packets with low processing priority
      packets_handlers->registerHandler<StatusPacketHandler>(config, peers_state, packets_stats, pbft_syncing_state,
                                                             pbft_chain, pbft_mgr, dag_mgr, db, genesis_hash, node_addr,
                                                             logs_prefix);
      packets_handlers->registerHandler<GetDagSyncPacketHandler>(config, peers_state, packets_stats, trx_mgr, dag_mgr,
                                                                 db, node_addr, logs_prefix);

      packets_handlers->registerHandler<DagSyncPacketHandler>(config, peers_state, packets_stats, pbft_syncing_state,
                                                              pbft_chain, pbft_mgr, dag_mgr, trx_mgr, db, node_addr,
                                                              logs_prefix);

      packets_handlers->registerHandler<GetPbftSyncPacketHandler>(
          config, peers_state, packets_stats, pbft_syncing_state, pbft_chain, vote_mgr, db, node_addr, logs_prefix);

      packets_handlers->registerHandler<PbftSyncPacketHandler>(config, peers_state, packets_stats, pbft_syncing_state,
                                                               pbft_chain, pbft_mgr, dag_mgr, vote_mgr, db, node_addr,
                                                               logs_prefix);

      return packets_handlers;
    };

}  // namespace taraxa::network::tarcap
