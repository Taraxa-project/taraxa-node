#include "network/network.hpp"

#include <libdevcrypto/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>

#include <boost/tokenizer.hpp>

#include "config/version.hpp"
#include "network/tarcap/packets_handlers/interface/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/interface/get_pillar_votes_bundle_packet_handler.hpp"
#include "network/tarcap/packets_handlers/interface/pillar_vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/interface/sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/interface/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/interface/vote_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "network/tarcap/stats/node_stats.hpp"
#include "network/tarcap/stats/time_period_packets_stats.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa {

Network::Network(const FullNodeConfig &config, const h256 &genesis_hash, const std::filesystem::path &network_file_path,
                 const dev::KeyPair &key, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr,
                 std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                 std::shared_ptr<SlashingManager> slashing_manager,
                 std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_mgr)
    : kConf(config),
      pub_key_(key.pub()),
      all_packets_stats_(nullptr),
      node_stats_(nullptr),
      pbft_syncing_state_(std::make_shared<network::tarcap::PbftSyncingState>(config.network.deep_syncing_threshold)),
      pbft_mgr_(pbft_mgr),
      tp_(config.network.num_threads, false),
      packets_tp_(std::make_shared<network::threadpool::PacketsThreadPool>(config.network.packets_processing_threads,
                                                                           pbft_mgr, key.address())),
      periodic_events_tp_(kPeriodicEventsThreadCount, false) {
  auto const &node_addr = key.address();
  LOG_OBJECTS_CREATE("NETWORK");
  LOG(log_nf_) << "Read Network Config: " << std::endl << config.network << std::endl;

  all_packets_stats_ = std::make_shared<network::tarcap::TimePeriodPacketsStats>(
      kConf.network.ddos_protection.packets_stats_time_period_ms, node_addr);

  node_stats_ =
      std::make_shared<network::tarcap::NodeStats>(pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr, vote_mgr,
                                                   trx_mgr, all_packets_stats_, packets_tp_, node_addr);

  // TODO make all these properties configurable
  dev::p2p::NetworkConfig net_conf;
  net_conf.listenIPAddress = config.network.listen_ip;
  net_conf.listenPort = config.network.listen_port;
  net_conf.discovery = true;
  net_conf.allowLocalDiscovery = true;
  net_conf.traverseNAT = false;
  net_conf.publicIPAddress = config.network.public_ip;
  net_conf.pin = false;
  net_conf.trustedNodes = config.network.trusted_nodes;

  dev::p2p::TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.ideal_peer_count = config.network.ideal_peer_count;

  // TODO config.network.max_peer_count -> config.network.peer_count_stretch
  taraxa_net_conf.peer_stretch = config.network.max_peer_count / config.network.ideal_peer_count;
  taraxa_net_conf.chain_id = config.genesis.chain_id;
  taraxa_net_conf.expected_parallelism = tp_.capacity();

  const std::string net_version = "TaraxaNode";

  // Create taraxa capabilities
  dev::p2p::Host::CapabilitiesFactory constructCapabilities = [&](std::weak_ptr<dev::p2p::Host> host) {
    assert(!host.expired());

    dev::p2p::Host::CapabilityList capabilities;

    // Register latest version of taraxa capability
    auto latest_tarcap = std::make_shared<network::tarcap::TaraxaCapability>(
        TARAXA_NET_VERSION, config, genesis_hash, host, key, packets_tp_, all_packets_stats_, pbft_syncing_state_, db,
        pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr, slashing_manager, pillar_chain_mgr);
    capabilities.emplace_back(latest_tarcap);

    // Register previous (v5) version of taraxa capability
    assert(TARAXA_NET_VERSION - 1 == 5);
    auto v5_tarcap = std::make_shared<network::tarcap::TaraxaCapability>(
        TARAXA_NET_VERSION - 1, config, genesis_hash, host, key, packets_tp_, all_packets_stats_, pbft_syncing_state_,
        db, pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr, slashing_manager, pillar_chain_mgr,
        network::tarcap::TaraxaCapability::kInitV5VersionHandlers);
    capabilities.emplace_back(v5_tarcap);

    return capabilities;
  };

  host_ = dev::p2p::Host::make(net_version, constructCapabilities, key, net_conf, taraxa_net_conf, network_file_path);
  for (const auto &cap : host_->getSupportedCapabilities()) {
    const auto tarcap_version = cap.second.ref->version();
    auto tarcap = std::static_pointer_cast<network::tarcap::TaraxaCapability>(cap.second.ref);
    tarcaps_[tarcap_version] = std::move(tarcap);
  }

  addBootNodes(true);

  // Register periodic events. Must be called after full init of tarcaps
  registerPeriodicEvents(pbft_mgr, trx_mgr);

  for (uint i = 0; i < tp_.capacity(); ++i) {
    tp_.post_loop({100 + i * 20}, [this] { while (0 < host_->do_work()); });
  }

  LOG(log_nf_) << "Configured host. Listening on address: " << config.network.listen_ip << ":"
               << config.network.listen_port;
}

Network::~Network() {
  tp_.stop();
  packets_tp_->stopProcessing();
  periodic_events_tp_.stop();
}

void Network::start() {
  packets_tp_->startProcessing();
  tp_.start();
  periodic_events_tp_.start();

  LOG(log_nf_) << "Started Node id: " << host_->id() << ", listening on port " << host_->listenPort();
}

bool Network::isStarted() { return tp_.is_running(); }

bool Network::packetQueueOverLimit() const {
  auto [hp_queue_size, mp_queue_size, lp_queue_size] = packets_tp_->getQueueSize();
  auto total_size = hp_queue_size + mp_queue_size + lp_queue_size;
  return total_size > kConf.network.ddos_protection.max_packets_queue_size;
}

std::list<dev::p2p::NodeEntry> Network::getAllNodes() const { return host_->getNodes(); }

size_t Network::getPeerCount() { return host_->peer_count(); }

unsigned Network::getNodeCount() { return host_->getNodeCount(); }

Json::Value Network::getStatus() {
  std::map<network::tarcap::TarcapVersion, std::shared_ptr<network::tarcap::TaraxaPeer>> peers;
  for (auto &tarcap : tarcaps_) {
    for (const auto &peer : tarcap.second->getPeersState()->getAllPeers()) {
      peers.emplace(tarcap.second->version(), std::move(peer.second));
    }
  }

  return node_stats_->getStatus(peers);
}

bool Network::pbft_syncing() { return pbft_syncing_state_->isPbftSyncing(); }

uint64_t Network::syncTimeSeconds() const {
  // TODO: this should be probably part of syncing_state, not node_stats
  return node_stats_->syncTimeSeconds();
}

void Network::setSyncStatePeriod(PbftPeriod period) { pbft_syncing_state_->setSyncStatePeriod(period); }

void Network::registerPeriodicEvents(const std::shared_ptr<PbftManager> &pbft_mgr,
                                     std::shared_ptr<TransactionManager> trx_mgr) {
  auto getAllPeers = [this]() {
    std::vector<std::shared_ptr<network::tarcap::TaraxaPeer>> all_peers;
    for (auto &tarcap : tarcaps_) {
      for (const auto &peer : tarcap.second->getPeersState()->getAllPeers()) {
        all_peers.push_back(std::move(peer.second));
      }
    }

    return all_peers;
  };

  uint64_t lambda_ms = pbft_mgr ? pbft_mgr->getPbftInitialLambda().count() : 2000;

  // Send new transactions
  auto sendTxs = [this, trx_mgr = trx_mgr]() {
    for (auto &tarcap : tarcaps_) {
      auto tx_packet_handler = tarcap.second->getSpecificHandler<network::tarcap::ITransactionPacketHandler>(
          network::SubprotocolPacketType::kTransactionPacket);
      tx_packet_handler->periodicSendTransactions(trx_mgr->getAllPoolTrxs());
    }
  };
  periodic_events_tp_.post_loop({kConf.network.transaction_interval_ms}, sendTxs);

  // Send status packet
  auto sendStatus = [this]() {
    for (auto &tarcap : tarcaps_) {
      auto status_packet_handler = tarcap.second->getSpecificHandler<network::tarcap::ISyncPacketHandler>(
          network::SubprotocolPacketType::kStatusPacket);
      status_packet_handler->sendStatusToPeers();
    }
  };

  const auto send_status_interval = 6 * lambda_ms;
  periodic_events_tp_.post_loop({send_status_interval}, sendStatus);

  // Check nodes connections and refresh boot nodes
  auto checkNodesConnections = [this]() {
    // If node count drops to zero add boot nodes again and retry
    if (host_->peer_count() == 0 || host_->getNodeCount() == 0) {
      addBootNodes();
    }
  };
  periodic_events_tp_.post_loop({30000}, checkNodesConnections);

  // Ddos protection stats
  auto ddosStats = [getAllPeers, ddos_protection = kConf.network.ddos_protection,
                    all_packets_stats = all_packets_stats_] {
    const auto all_peers = getAllPeers();

    // Log interval + max packets stats only if enabled in config
    if (ddos_protection.log_packets_stats) {
      all_packets_stats->processStats(all_peers);
    }

    // Per peer packets stats are used for ddos protection
    for (const auto &peer : all_peers) {
      peer->resetPacketsStats();
    }
  };
  periodic_events_tp_.post_loop(
      {static_cast<uint64_t>(kConf.network.ddos_protection.packets_stats_time_period_ms.count())}, ddosStats);

  // SUMMARY log
  const auto node_stats_log_interval = 5 * 6 * lambda_ms;
  auto summaryLog = [getAllPeers, node_stats = node_stats_, host = host_]() {
    std::vector<std::string> nodes_addresses;
    auto nodes = host->getNodes();
    nodes_addresses.reserve(nodes.size());
    for (auto node : nodes) {
      auto endpoint = node.endpoint();
      if (endpoint.tcpPort() == endpoint.udpPort())
        nodes_addresses.push_back(node.id().abridged() + ":" + endpoint.address().to_string() + ":" +
                                  std::to_string(endpoint.tcpPort()));
      else
        nodes_addresses.push_back(node.id().abridged() + ":" + endpoint.address().to_string() + ":" +
                                  std::to_string(endpoint.tcpPort()) + ":" + std::to_string(endpoint.udpPort()));
    }
    node_stats->logNodeStats(getAllPeers(), nodes_addresses);
  };
  periodic_events_tp_.post_loop({node_stats_log_interval}, summaryLog);
}

void Network::addBootNodes(bool initial) {
  auto resolveHost = [](const std::string &addr, uint16_t port) {
    static boost::asio::io_context s_resolverIoService;
    boost::system::error_code ec;
    bi::address address = bi::make_address(addr, ec);
    bi::tcp::endpoint ep(bi::address(), port);
    if (!ec) {
      ep.address(address);
    } else {
      // resolve returns an iterator (host can resolve to multiple addresses)
      bi::tcp::resolver r(s_resolverIoService);
      auto it = r.resolve(bi::tcp::v4(), addr, std::to_string(port), ec);
      if (ec) {
        return std::make_pair(false, bi::tcp::endpoint());
      } else {
        ep = *it.begin();
      }
    }
    return std::make_pair(true, ep);
  };

  for (auto const &node : kConf.network.boot_nodes) {
    dev::Public pub(node.id);
    if (pub == pub_key_) {
      LOG(log_wr_) << "not adding self to the boot node list";
      continue;
    }

    if (host_->nodeTableHasNode(pub)) {
      LOG(log_dg_) << "skipping node " << node.id << " already in table";
      continue;
    }

    auto ip = resolveHost(node.ip, node.port);
    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.port << " " << ip.second.address().to_string();
    dev::p2p::Node boot_node(pub, dev::p2p::NodeIPEndpoint(ip.second.address(), node.port, node.port),
                             dev::p2p::PeerType::Required);
    host_->addNode(boot_node);
    if (!initial) {
      host_->invalidateNode(boot_node.id);
    }
  }
}

void Network::gossipDagBlock(const std::shared_ptr<DagBlock> &block, bool proposed, const SharedTransactions &trxs) {
  for (const auto &tarcap : tarcaps_) {
    auto dag_block_packet_handler = tarcap.second->getSpecificHandler<network::tarcap::IDagBlockPacketHandler>(
        network::SubprotocolPacketType::kDagBlockPacket);
    dag_block_packet_handler->onNewBlockVerified(block, proposed, trxs);
  }
}

void Network::gossipVote(const std::shared_ptr<PbftVote> &vote, const std::shared_ptr<PbftBlock> &block,
                         bool rebroadcast) {
  for (const auto &tarcap : tarcaps_) {
    auto vote_packet_handler = tarcap.second->getSpecificHandler<network::tarcap::IVotePacketHandler>(
        network::SubprotocolPacketType::kVotePacket);
    vote_packet_handler->onNewPbftVote(vote, block, rebroadcast);
  }
}

void Network::gossipVotesBundle(const std::vector<std::shared_ptr<PbftVote>> &votes, bool rebroadcast) {
  for (const auto &tarcap : tarcaps_) {
    tarcap.second
        ->getSpecificHandler<network::tarcap::IVotePacketHandler>(network::SubprotocolPacketType::kVotesBundlePacket)
        ->onNewPbftVotesBundle(votes, rebroadcast);
  }
}

void Network::gossipPillarBlockVote(const std::shared_ptr<PillarVote> &vote, bool rebroadcast) {
  for (const auto &tarcap : tarcaps_) {
    auto pillar_vote_packet_handler = tarcap.second->getSpecificHandler<network::tarcap::IPillarVotePacketHandler>(
        network::SubprotocolPacketType::kPillarVotePacket);
    pillar_vote_packet_handler->onNewPillarVote(vote, rebroadcast);
  }
}

void Network::handleMaliciousSyncPeer(const dev::p2p::NodeID &node_id) {
  for (const auto &tarcap : tarcaps_) {
    auto peers_state = tarcap.second->getPeersState();
    // Peer is present only in one taraxa capability depending on his network version
    if (auto peer = peers_state->getPeer(node_id); !peer) {
      continue;
    }

    peers_state->handleMaliciousSyncPeer(node_id);
  }
}

std::shared_ptr<network::tarcap::TaraxaPeer> Network::getMaxChainPeer() const {
  std::shared_ptr<network::tarcap::TaraxaPeer> max_chain_peer{nullptr};

  for (const auto &tarcap : tarcaps_) {
    const auto peer = tarcap.second->getPeersState()->getMaxChainPeer(pbft_mgr_);
    if (!peer) {
      continue;
    }

    if (!max_chain_peer) {
      max_chain_peer = peer;
    } else if (peer->pbft_chain_size_ > max_chain_peer->pbft_chain_size_) {
      max_chain_peer = peer;
    }
  }

  return max_chain_peer;
}

void Network::requestPillarBlockVotesBundle(taraxa::PbftPeriod period, const taraxa::blk_hash_t &pillar_block_hash) {
  // Max peer among all tarcaps
  const auto max_peer = getMaxChainPeer();

  for (const auto &tarcap : tarcaps_) {
    // Try to get most up-to-date peer
    const auto peer = tarcap.second->getPeersState()->getMaxChainPeer(pbft_mgr_);
    if (!peer) {
      continue;
    }

    if (peer->getId() != max_peer->getId()) {
      continue;
    }

    auto get_pillar_votes_bundle_packet_handler =
        tarcap.second->getSpecificHandler<network::tarcap::IGetPillarVotesBundlePacketHandler>(
            network::SubprotocolPacketType::kGetPillarVotesBundlePacket);
    get_pillar_votes_bundle_packet_handler->requestPillarVotesBundle(period, pillar_block_hash, peer);
  }
}

// METHODS USED IN TESTS ONLY
// Note: for functions use in tests all data are fetched only from the tarcap with the highest version,
//       other functions must use all tarcaps
dev::p2p::NodeID Network::getNodeId() const { return host_->id(); }

std::shared_ptr<network::tarcap::TaraxaPeer> Network::getPeer(dev::p2p::NodeID const &id) const {
  return tarcaps_.begin()->second->getPeersState()->getPeer(id);
}
// METHODS USED IN TESTS ONLY

}  // namespace taraxa
