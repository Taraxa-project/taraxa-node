#include "network/tarcap/taraxa_capability.hpp"

#include <algorithm>

#include "dag/dag.hpp"
#include "network/tarcap/packets_handler.hpp"
#include "network/tarcap/packets_handlers/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/get_next_votes_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/get_pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/status_packet_handler.hpp"
#include "network/tarcap/packets_handlers/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "network/tarcap/stats/node_stats.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "node/node.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote/vote.hpp"

namespace taraxa::network::tarcap {

TaraxaCapability::TaraxaCapability(std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key,
                                   const FullNodeConfig &conf, unsigned version)
    : test_state_(std::make_shared<TestState>()),
      version_(version),
      kConf(conf),
      peers_state_(nullptr),
      pbft_syncing_state_(std::make_shared<PbftSyncingState>(conf.network.deep_syncing_threshold)),
      node_stats_(nullptr),
      packets_handlers_(std::make_shared<PacketsHandler>()),
      thread_pool_(std::make_shared<TarcapThreadPool>(conf.network.packets_processing_threads, key.address())),
      periodic_events_tp_(std::make_shared<util::ThreadPool>(kPeriodicEventsThreadCount, false)),
      pub_key_(key.pub()) {
  const auto &node_addr = key.address();
  LOG_OBJECTS_CREATE("TARCAP");

  peers_state_ = std::make_shared<PeersState>(host, kConf);
  all_packets_stats_ =
      std::make_shared<TimePeriodPacketsStats>(kConf.network.ddos_protection.packets_stats_time_period_ms, node_addr);

  // Inits boot nodes (based on config)
  addBootNodes(true);
}

std::shared_ptr<TaraxaCapability> TaraxaCapability::make(
    std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key, const FullNodeConfig &conf, const h256 &genesis_hash,
    unsigned version, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr,
    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
    std::shared_ptr<TransactionManager> trx_mgr) {
  auto instance = std::make_shared<TaraxaCapability>(host, key, conf, version);
  instance->init(genesis_hash, db, pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr, key.address());
  return instance;
}

void TaraxaCapability::init(const h256 &genesis_hash, std::shared_ptr<DbStorage> db,
                            std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                            std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                            std::shared_ptr<TransactionManager> trx_mgr, const dev::Address &node_addr) {
  // Creates and registers all packets handlers
  registerPacketHandlers(genesis_hash, all_packets_stats_, db, pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr,
                         node_addr);

  // Inits periodic events. Must be called after registerHandlers !!!
  initPeriodicEvents(pbft_mgr, trx_mgr, all_packets_stats_);
}

void TaraxaCapability::addBootNodes(bool initial) {
  auto resolveHost = [](const std::string &addr, uint16_t port) {
    static boost::asio::io_context s_resolverIoService;
    boost::system::error_code ec;
    bi::address address = bi::address::from_string(addr, ec);
    bi::tcp::endpoint ep(bi::address(), port);
    if (!ec) {
      ep.address(address);
    } else {
      // resolve returns an iterator (host can resolve to multiple addresses)
      bi::tcp::resolver r(s_resolverIoService);
      auto it = r.resolve({bi::tcp::v4(), addr, std::to_string(port)}, ec);
      if (ec) {
        return std::make_pair(false, bi::tcp::endpoint());
      } else {
        ep = *it;
      }
    }
    return std::make_pair(true, ep);
  };

  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "Unable to obtain host in addBootNodes";
    return;
  }

  for (auto const &node : kConf.network.boot_nodes) {
    dev::Public pub(node.id);
    if (pub == pub_key_) {
      LOG(log_wr_) << "not adding self to the boot node list";
      continue;
    }

    if (host->nodeTableHasNode(pub)) {
      LOG(log_dg_) << "skipping node " << node.id << " already in table";
      continue;
    }

    auto ip = resolveHost(node.ip, node.port);
    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.port << " " << ip.second.address().to_string();
    dev::p2p::Node boot_node(pub, dev::p2p::NodeIPEndpoint(ip.second.address(), node.port, node.port),
                             dev::p2p::PeerType::Required);
    host->addNode(boot_node);
    if (!initial) {
      host->invalidateNode(boot_node.id);
    }
  }
}

void TaraxaCapability::initPeriodicEvents(const std::shared_ptr<PbftManager> &pbft_mgr,
                                          std::shared_ptr<TransactionManager> trx_mgr,
                                          std::shared_ptr<TimePeriodPacketsStats> packets_stats) {
  // TODO: refactor this:
  //       1. Most of time is this single threaded thread pool doing nothing...
  //       2. These periodic events are sending packets - that might be processed by main thread_pool ???
  // Creates periodic events
  uint64_t lambda_ms = pbft_mgr ? pbft_mgr->getPbftInitialLambda().count() : 2000;

  // Send new txs periodic event
  auto tx_packet_handler = packets_handlers_->getSpecificHandler<TransactionPacketHandler>();
  if (trx_mgr /* just because of tests */ && kConf.network.transaction_interval_ms > 0) {
    periodic_events_tp_->post_loop({kConf.network.transaction_interval_ms},
                                   [tx_packet_handler = std::move(tx_packet_handler), trx_mgr = std::move(trx_mgr)] {
                                     tx_packet_handler->periodicSendTransactions(trx_mgr->getAllPoolTrxs());
                                   });
  }

  // Send status periodic event
  auto status_packet_handler = packets_handlers_->getSpecificHandler<StatusPacketHandler>();
  const auto send_status_interval = 6 * lambda_ms;
  periodic_events_tp_->post_loop({send_status_interval}, [status_packet_handler = std::move(status_packet_handler)] {
    status_packet_handler->sendStatusToPeers();
  });

  periodic_events_tp_->post_loop(
      {static_cast<uint64_t>(kConf.network.ddos_protection.packets_stats_time_period_ms.count())},
      [ddos_protection = kConf.network.ddos_protection, stats = packets_stats, peers_state = peers_state_] {
        // Log interval + max packets stats only if enabled in config
        if (ddos_protection.log_packets_stats) {
          stats->processStats(peers_state);
        }

        // Per peer packets stats are used for ddos protection
        for (const auto &peer : peers_state->getAllPeers()) {
          peer.second->resetPacketsStats();
        }
      });

  // SUMMARY log periodic event
  const auto node_stats_log_interval = 5 * 6 * lambda_ms;
  periodic_events_tp_->post_loop({node_stats_log_interval},
                                 [node_stats = node_stats_]() mutable { node_stats->logNodeStats(); });

  // Every 30 seconds check if connected to another node and refresh boot nodes
  periodic_events_tp_->post_loop({30000}, [this] {
    auto host = peers_state_->host_.lock();
    if (!host) {
      LOG(log_er_) << "Unable to obtain host in initPeriodicEvents";
      return;
    }
    // If node count drops to zero add boot nodes again and retry
    if (host->peer_count() == 0) {
      addBootNodes();
    }
  });
}

void TaraxaCapability::registerPacketHandlers(
    const h256 &genesis_hash, const std::shared_ptr<TimePeriodPacketsStats> &packets_stats,
    const std::shared_ptr<DbStorage> &db, const std::shared_ptr<PbftManager> &pbft_mgr,
    const std::shared_ptr<PbftChain> &pbft_chain, const std::shared_ptr<VoteManager> &vote_mgr,
    const std::shared_ptr<DagManager> &dag_mgr, const std::shared_ptr<TransactionManager> &trx_mgr,
    const addr_t &node_addr) {
  node_stats_ = std::make_shared<NodeStats>(peers_state_, pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr, vote_mgr,
                                            trx_mgr, packets_stats, thread_pool_, node_addr);

  // Register all packet handlers

  // Consensus packets with high processing priority
  packets_handlers_->registerHandler<VotePacketHandler>(kConf, peers_state_, packets_stats, pbft_mgr, pbft_chain,
                                                        vote_mgr, node_addr);
  packets_handlers_->registerHandler<GetNextVotesSyncPacketHandler>(kConf, peers_state_, packets_stats, pbft_mgr,
                                                                    pbft_chain, vote_mgr, node_addr);
  packets_handlers_->registerHandler<VotesSyncPacketHandler>(kConf, peers_state_, packets_stats, pbft_mgr, pbft_chain,
                                                             vote_mgr, node_addr);

  // Standard packets with mid processing priority
  packets_handlers_->registerHandler<DagBlockPacketHandler>(kConf, peers_state_, packets_stats, pbft_syncing_state_,
                                                            pbft_chain, pbft_mgr, dag_mgr, trx_mgr, db, test_state_,
                                                            node_addr);

  packets_handlers_->registerHandler<TransactionPacketHandler>(kConf, peers_state_, packets_stats, trx_mgr, test_state_,
                                                               node_addr);

  // Non critical packets with low processing priority
  packets_handlers_->registerHandler<StatusPacketHandler>(kConf, peers_state_, packets_stats, pbft_syncing_state_,
                                                          pbft_chain, pbft_mgr, dag_mgr, db, genesis_hash, node_addr);
  packets_handlers_->registerHandler<GetDagSyncPacketHandler>(kConf, peers_state_, packets_stats, trx_mgr, dag_mgr, db,
                                                              node_addr);

  packets_handlers_->registerHandler<DagSyncPacketHandler>(kConf, peers_state_, packets_stats, pbft_syncing_state_,
                                                           pbft_chain, pbft_mgr, dag_mgr, trx_mgr, db, node_addr);

  // TODO there is additional logic, that should be moved outside process function
  packets_handlers_->registerHandler<GetPbftSyncPacketHandler>(kConf, peers_state_, packets_stats, pbft_syncing_state_,
                                                               pbft_chain, vote_mgr, db, node_addr);

  packets_handlers_->registerHandler<PbftSyncPacketHandler>(kConf, peers_state_, packets_stats, pbft_syncing_state_,
                                                            pbft_chain, pbft_mgr, dag_mgr, vote_mgr,
                                                            periodic_events_tp_, db, node_addr);

  thread_pool_->setPacketsHandlers(packets_handlers_);
}

std::string TaraxaCapability::name() const { return TARAXA_CAPABILITY_NAME; }

unsigned TaraxaCapability::version() const { return version_; }

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
    if (peers_state_->getPeersCount() > 0) {
      LOG(log_dg_) << "Restart PBFT/DAG syncing due to syncing peer disconnect.";
      packets_handlers_->getSpecificHandler<PbftSyncPacketHandler>()->restartSyncingPbft(true);
    } else {
      LOG(log_dg_) << "Stop PBFT/DAG syncing due to syncing peer disconnect and no other peers available.";
      pbft_syncing_state_->setPbftSyncing(false);
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
  thread_pool_->push(PacketData(packet_type, node_id, _r.data().toBytes()));
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

void TaraxaCapability::start() {
  thread_pool_->startProcessing();
  periodic_events_tp_->start();
}

void TaraxaCapability::stop() {
  thread_pool_->stopProcessing();
  periodic_events_tp_->stop();
}

const std::shared_ptr<PeersState> &TaraxaCapability::getPeersState() { return peers_state_; }

const std::shared_ptr<NodeStats> &TaraxaCapability::getNodeStats() { return node_stats_; }

bool TaraxaCapability::pbft_syncing() const { return pbft_syncing_state_->isPbftSyncing(); }

void TaraxaCapability::setSyncStatePeriod(PbftPeriod period) { pbft_syncing_state_->setSyncStatePeriod(period); }

// METHODS USED IN TESTS ONLY
size_t TaraxaCapability::getReceivedBlocksCount() const { return test_state_->getBlocksSize(); }

size_t TaraxaCapability::getReceivedTransactionsCount() const { return test_state_->getTransactionsSize(); }
// END METHODS USED IN TESTS ONLY

}  // namespace taraxa::network::tarcap
