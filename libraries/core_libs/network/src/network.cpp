#include "network/network.hpp"

#include <libdevcrypto/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>

#include <boost/tokenizer.hpp>
#include <ranges>

#include "config/version.hpp"
#include "network/tarcap/capability_latest/packets_handlers/dag_block_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/pbft_sync_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/votes_bundle_packet_handler.hpp"
#include "network/tarcap/capability_latest/taraxa_capability.hpp"
#include "network/tarcap/capability_v1/taraxa_capability.hpp"

namespace taraxa {

Network::Network(const FullNodeConfig &config, const h256 &genesis_hash,
                 dev::p2p::Host::CapabilitiesFactory construct_capabilities,
                 std::filesystem::path const &network_file_path, dev::KeyPair const &key, std::shared_ptr<DbStorage> db,
                 std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                 std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                 std::shared_ptr<TransactionManager> trx_mgr)
    : tp_(config.network.num_threads, false),
      packets_tp_(std::make_shared<network::threadpool::PacketsThreadPool>(config.network.packets_processing_threads,
                                                                           key.address())) {
  auto const &node_addr = key.address();
  LOG_OBJECTS_CREATE("NETWORK");
  LOG(log_nf_) << "Read Network Config: " << std::endl << config.network << std::endl;

  // TODO make all these properties configurable
  dev::p2p::NetworkConfig net_conf;
  net_conf.listenIPAddress = config.network.listen_ip;
  net_conf.listenPort = config.network.listen_port;
  net_conf.discovery = true;
  net_conf.allowLocalDiscovery = true;
  net_conf.traverseNAT = false;
  net_conf.publicIPAddress = config.network.public_ip;
  net_conf.pin = false;

  dev::p2p::TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.ideal_peer_count = config.network.ideal_peer_count;

  // TODO config.network.max_peer_count -> config.network.peer_count_stretch
  taraxa_net_conf.peer_stretch = config.network.max_peer_count / config.network.ideal_peer_count;
  taraxa_net_conf.chain_id = config.genesis.chain_id;
  taraxa_net_conf.expected_parallelism = tp_.capacity();

  string net_version = "TaraxaNode";  // TODO maybe give a proper name?
  if (!construct_capabilities) {
    construct_capabilities = [&](std::weak_ptr<dev::p2p::Host> host) {
      assert(!host.expired());

      const size_t kOldNetworkVersion = 1;
      assert(kOldNetworkVersion < TARAXA_NET_VERSION);

      dev::p2p::Host::CapabilityList capabilities;

      // Register old version of taraxa capability
      auto v1_tarcap =
          std::make_shared<network::tarcap::v1::TaraxaCapability>(host, key, config, kOldNetworkVersion, "V1_TARCAP");
      v1_tarcap->init(genesis_hash, db, pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr, key.address());
      capabilities.emplace_back(v1_tarcap);

      // Register new version of taraxa capability
      auto v2_tarcap =
          std::make_shared<network::tarcap::TaraxaCapability>(host, key, config, TARAXA_NET_VERSION, "TARCAP");
      v2_tarcap->init(genesis_hash, db, pbft_mgr, pbft_chain, vote_mgr, dag_mgr, trx_mgr, key.address());
      capabilities.emplace_back(v2_tarcap);

      return capabilities;
    };
  }

  host_ = dev::p2p::Host::make(net_version, construct_capabilities, key, net_conf, taraxa_net_conf, network_file_path);
  for (const auto &cap : host_->getSupportedCapabilities()) {
    const auto tarcap_version = cap.second.ref->version();
    auto tarcap = std::static_pointer_cast<network::tarcap::TaraxaCapabilityBase>(cap.second.ref);
    packets_tp_->setPacketsHandlers(tarcap_version, tarcap->getPacketsHandler());

    tarcap->setThreadPool(packets_tp_);
    tarcaps_[tarcap_version] = std::move(tarcap);
  }

  for (uint i = 0; i < tp_.capacity(); ++i) {
    tp_.post_loop({100 + i * 20}, [this] {
      while (0 < host_->do_work())
        ;
    });
  }

  LOG(log_nf_) << "Configured host. Listening on address: " << config.network.listen_ip << ":"
               << config.network.listen_port;
}

Network::~Network() {
  tp_.stop();
  packets_tp_->stopProcessing();
  //  periodic_events_tp_.stop();

  // TODO: remove once packets_tp_ and periodic_events_tp_ are moved from tarcaps to network
  for (auto &tarcap : tarcaps_) {
    tarcap.second->stop();
  }
}

void Network::start() {
  packets_tp_->startProcessing();
  //  periodic_events_tp_.start();

  // TODO: remove once packets_tp_ and periodic_events_tp_ are moved from tarcaps to network
  for (auto &tarcap : tarcaps_) {
    tarcap.second->start();
  }

  tp_.start();
  LOG(log_nf_) << "Started Node id: " << host_->id() << ", listening on port " << host_->listenPort();
}

bool Network::isStarted() { return tp_.is_running(); }

std::list<dev::p2p::NodeEntry> Network::getAllNodes() const { return host_->getNodes(); }

size_t Network::getPeerCount() { return host_->peer_count(); }

unsigned Network::getNodeCount() { return host_->getNodeCount(); }

Json::Value Network::getStatus() {
  // TODO: refactor this: combine node stats from all tarcaps...
  return tarcaps_.end()->second->getNodeStats()->getStatus();
}

bool Network::pbft_syncing() {
  return std::ranges::any_of(tarcaps_, [](const auto &tarcap) { return tarcap.second->pbft_syncing(); });
}


void Network::setSyncStatePeriod(PbftPeriod period) {
  for (auto &tarcap : tarcaps_) {
    // TODO: double check this ???
    if (tarcap.second->pbft_syncing()) {
      tarcap.second->setSyncStatePeriod(period);
    }
  }
}

void Network::gossipDagBlock(const DagBlock &block, bool proposed, const SharedTransactions &trxs) {
  for (const auto &tarcap : tarcaps_ | std::views::reverse) {
    tarcap.second->getSpecificHandler<network::tarcap::DagBlockPacketHandler>()->onNewBlockVerified(block, proposed,
                                                                                                    trxs);
  }
}

void Network::gossipVote(const std::shared_ptr<Vote> &vote, const std::shared_ptr<PbftBlock> &block, bool rebroadcast) {
  for (const auto &tarcap : tarcaps_ | std::views::reverse) {
    tarcap.second->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVote(vote, block, rebroadcast);
  }
}

void Network::gossipVotesBundle(const std::vector<std::shared_ptr<Vote>> &votes, bool rebroadcast) {
  for (const auto &tarcap : tarcaps_ | std::views::reverse) {
    tarcap.second->getSpecificHandler<network::tarcap::VotesBundlePacketHandler>()->onNewPbftVotesBundle(votes,
                                                                                                         rebroadcast);
  }
}

void Network::handleMaliciousSyncPeer(const dev::p2p::NodeID &node_id) {
  for (const auto &tarcap : tarcaps_ | std::views::reverse) {
    // Peer is present only in one taraxa capability depending on his network version
    if (auto peer = tarcap.second->getPeersState()->getPeer(node_id); !peer) {
      continue;
    }

    tarcap.second->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
  }
}

std::shared_ptr<network::tarcap::TaraxaPeer> Network::getMaxChainPeer() const {
  std::shared_ptr<network::tarcap::TaraxaPeer> max_chain_peer{nullptr};

  for (const auto &tarcap : tarcaps_ | std::views::reverse) {
    const auto peer =
        tarcap.second->getSpecificHandler<::taraxa::network::tarcap::PbftSyncPacketHandler>()->getMaxChainPeer();
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

// METHODS USED IN TESTS ONLY
// Note: for functions use in tests all data are fetched only from the tarcap with the highest version,
//       other functions must use all tarcaps

void Network::setPendingPeersToReady() {
  const auto &peers_state = tarcaps_.rbegin()->second->getPeersState();

  auto peerIds = peers_state->getAllPendingPeersIDs();
  for (const auto &peerId : peerIds) {
    auto peer = peers_state->getPendingPeer(peerId);
    if (peer) {
      peers_state->setPeerAsReadyToSendMessages(peerId, peer);
    }
  }
}

dev::p2p::NodeID Network::getNodeId() const { return host_->id(); }

int Network::getReceivedBlocksCount() const { return tarcaps_.rbegin()->second->getReceivedBlocksCount(); }

int Network::getReceivedTransactionsCount() const { return tarcaps_.rbegin()->second->getReceivedTransactionsCount(); }

std::shared_ptr<network::tarcap::TaraxaPeer> Network::getPeer(dev::p2p::NodeID const &id) const {
  return tarcaps_.rbegin()->second->getPeersState()->getPeer(id);
}
// METHODS USED IN TESTS ONLY

}  // namespace taraxa
