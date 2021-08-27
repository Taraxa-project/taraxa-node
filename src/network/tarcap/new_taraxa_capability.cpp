#include "new_taraxa_capability.hpp"

#include <algorithm>

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/blocks_packet_handler.hpp"
#include "network/tarcap/packets_handlers/common/syncing_handler.hpp"
#include "network/tarcap/packets_handlers/dag_packets_handler.hpp"
#include "network/tarcap/packets_handlers/get_blocks_packet_handler.hpp"
#include "network/tarcap/packets_handlers/get_pbft_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/new_pbft_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/status_packet_handler.hpp"
#include "network/tarcap/packets_handlers/test_packet_handler.hpp"
#include "network/tarcap/packets_handlers/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/vote_packets_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "network/tarcap/stats/node_stats.hpp"
#include "node/full_node.hpp"
#include "packets_handler.hpp"
#include "taraxa_peer.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

TaraxaCapability::TaraxaCapability(std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key,
                                   const NetworkConfig &conf, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                   std::shared_ptr<VoteManager> vote_mgr,
                                   std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr,
                                   std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                   std::shared_ptr<TransactionManager> trx_mgr, addr_t const &node_addr)
    : peers_state_(std::make_shared<PeersState>(std::move(host))),
      syncing_state_(std::make_shared<SyncingState>(peers_state_)),
      syncing_handler_(nullptr),
      test_state_(std::make_shared<TestState>()),
      node_stats_(nullptr),
      packets_handlers_(std::make_shared<PacketsHandler>()),
      thread_pool_(conf.network_packets_processing_threads, node_addr),
      periodic_events_tp_(1, false) {
  LOG_OBJECTS_CREATE("TARCAP");

  auto packets_stats = std::make_shared<PacketsStats>(node_addr);

  // Inits boot nodes (based on config)
  initBootNodes(conf.network_boot_nodes, key);

  // Creates and registers all packets handlers
  registerPacketHandlers(conf, packets_stats, db, pbft_mgr, pbft_chain, vote_mgr, next_votes_mgr, dag_mgr, dag_blk_mgr,
                         trx_mgr, node_addr);

  // Inits periodic events. Must be called after registerHandlers !!!
  initPeriodicEvents(conf, pbft_mgr, trx_mgr, packets_stats);
}

void TaraxaCapability::initBootNodes(const std::vector<NodeConfig> &network_boot_nodes, const dev::KeyPair &key) {
  auto resolveHost = [](string const &addr, uint16_t port) {
    static boost::asio::io_context s_resolverIoService;
    boost::system::error_code ec;
    bi::address address = bi::address::from_string(addr, ec);
    bi::tcp::endpoint ep(bi::address(), port);
    if (!ec) {
      ep.address(address);
    } else {
      boost::system::error_code ec;
      // resolve returns an iterator (host can resolve to multiple addresses)
      bi::tcp::resolver r(s_resolverIoService);
      auto it = r.resolve({bi::tcp::v4(), addr, toString(port)}, ec);
      if (ec) {
        return std::make_pair(false, bi::tcp::endpoint());
      } else {
        ep = *it;
      }
    }
    return std::make_pair(true, ep);
  };

  for (auto const &node : network_boot_nodes) {
    Public pub(node.id);
    if (pub == key.pub()) {
      LOG(log_wr_) << "not adding self to the boot node list";
      continue;
    }

    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.tcp_port;
    auto ip = resolveHost(node.ip, node.tcp_port);
    boot_nodes_[pub] = dev::p2p::NodeIPEndpoint(ip.second.address(), node.tcp_port, node.tcp_port);
  }

  LOG(log_nf_) << " Number of boot node added: " << boot_nodes_.size() << std::endl;
}

void TaraxaCapability::initPeriodicEvents(const NetworkConfig &conf, const std::shared_ptr<PbftManager> &pbft_mgr,
                                          std::shared_ptr<TransactionManager> trx_mgr,
                                          std::shared_ptr<PacketsStats> packets_stats) {
  // TODO: refactor this:
  //       1. Most of time is this single threaded thread pool doing nothing...
  //       2. These periodic events are sending packets - that might be processed by main thread_pool ???
  // Creates periodic events
  const auto lambda_ms_min = pbft_mgr ? pbft_mgr->getPbftInitialLambda() : 2000;

  // Send new txs periodic event
  const auto &tx_handler = packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_TransactionPacket);
  auto tx_packet_handler = std::static_pointer_cast<TransactionPacketHandler>(tx_handler);
  if (trx_mgr /* just because of tests */ && conf.network_transaction_interval > 0) {
    periodic_events_tp_.post_loop({conf.network_transaction_interval},
                                  [tx_packet_handler = std::move(tx_packet_handler), trx_mgr = std::move(trx_mgr)] {
                                    tx_packet_handler->onNewTransactions(trx_mgr->getNewVerifiedTrxSnapShotSerialized(),
                                                                         false);
                                  });
  }

  // Send status periodic event
  const auto &status_handler = packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_StatusPacket);
  auto status_packet_handler = std::static_pointer_cast<StatusPacketHandler>(status_handler);
  const auto send_status_interval = 6 * lambda_ms_min;
  periodic_events_tp_.post_loop({send_status_interval}, [status_packet_handler = std::move(status_packet_handler)] {
    status_packet_handler->sendStatusToPeers();
  });

  // Logs packets stats periodic event
  if (conf.network_performance_log_interval > 0) {
    periodic_events_tp_.post_loop({conf.network_performance_log_interval},
                                  [packets_stats = std::move(packets_stats)] { packets_stats->logStats(); });
  }

  // SUMMARY log periodic event
  const auto node_stats_log_interval = 5 * 6 * lambda_ms_min;
  periodic_events_tp_.post_loop({node_stats_log_interval},
                                [node_stats = node_stats_]() mutable { node_stats->logNodeStats(); });

  // Boot nodes checkup periodic event
  if (!boot_nodes_.empty()) {
    auto tmp_host = peers_state_->host_.lock();
    for (auto const &[k, v] : boot_nodes_) {
      tmp_host->addNode(dev::p2p::Node(k, v, dev::p2p::PeerType::Required));
    }

    // Every 30 seconds check if connected to another node and refresh boot nodes
    periodic_events_tp_.post_loop({30000}, [this] {
      auto host = peers_state_->host_.lock();

      // If node count drops to zero add boot nodes again and retry
      if (host->getNodeCount() == 0) {
        for (auto const &[k, v] : boot_nodes_) {
          host->addNode(dev::p2p::Node(k, v, dev::p2p::PeerType::Required));
        }
      }
      if (host->peer_count() == 0) {
        for (auto const &[k, _] : boot_nodes_) {
          host->invalidateNode(k);
        }
      }
    });
  }
}

void TaraxaCapability::registerPacketHandlers(
    const NetworkConfig &conf, const std::shared_ptr<PacketsStats> &packets_stats, const std::shared_ptr<DbStorage> &db,
    const std::shared_ptr<PbftManager> &pbft_mgr, const std::shared_ptr<PbftChain> &pbft_chain,
    const std::shared_ptr<VoteManager> &vote_mgr, const std::shared_ptr<NextVotesForPreviousRound> &next_votes_mgr,
    const std::shared_ptr<DagManager> &dag_mgr, const std::shared_ptr<DagBlockManager> &dag_blk_mgr,
    const std::shared_ptr<TransactionManager> &trx_mgr, addr_t const &node_addr) {
  syncing_handler_ = std::make_shared<SyncingHandler>(peers_state_, packets_stats, syncing_state_, pbft_chain, dag_mgr,
                                                      dag_blk_mgr, node_addr);

  node_stats_ = std::make_shared<NodeStats>(peers_state_, syncing_state_, pbft_chain, pbft_mgr, dag_mgr, dag_blk_mgr,
                                            vote_mgr, trx_mgr, packets_stats);

  // Register all packet handlers
  // Consensus packets with high processing priority
  const auto votes_handler = std::make_shared<VotePacketsHandler>(peers_state_, packets_stats, pbft_mgr, vote_mgr,
                                                                  next_votes_mgr, db, node_addr);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_PbftVotePacket, votes_handler);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_GetPbftNextVotes, votes_handler);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_PbftNextVotesPacket, votes_handler);

  // Standard packets with mid processing priority
  packets_handlers_->registerHandler(
      PriorityQueuePacketType::PQ_NewPbftBlockPacket,
      std::make_shared<NewPbftBlockPacketHandler>(peers_state_, packets_stats, pbft_chain, node_addr));

  const auto dag_handler = std::make_shared<DagPacketsHandler>(
      peers_state_, packets_stats, syncing_state_, syncing_handler_, trx_mgr, dag_blk_mgr, db, test_state_,
      conf.network_min_dag_block_broadcast, conf.network_max_dag_block_broadcast, node_addr);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_NewBlockPacket, dag_handler);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_NewBlockHashPacket, dag_handler);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_GetNewBlockPacket, dag_handler);

  packets_handlers_->registerHandler(
      PriorityQueuePacketType::PQ_TransactionPacket,
      std::make_shared<TransactionPacketHandler>(peers_state_, packets_stats, trx_mgr, dag_blk_mgr, test_state_,
                                                 conf.network_transaction_interval, node_addr));

  // Non critical packets with low processing priority
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_TestPacket,
                                     std::make_shared<TestPacketHandler>(peers_state_, packets_stats, node_addr));
  packets_handlers_->registerHandler(
      PriorityQueuePacketType::PQ_StatusPacket,
      std::make_shared<StatusPacketHandler>(peers_state_, packets_stats, syncing_state_, syncing_handler_, pbft_chain,
                                            dag_mgr, next_votes_mgr, pbft_mgr, conf.network_id, node_addr));
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_GetBlocksPacket,
                                     std::make_shared<GetBlocksPacketsHandler>(peers_state_, packets_stats, trx_mgr,
                                                                               dag_mgr, dag_blk_mgr, db, node_addr));

  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_BlocksPacket,
                                     std::make_shared<BlocksPacketHandler>(peers_state_, packets_stats, syncing_state_,
                                                                           syncing_handler_, dag_blk_mgr, node_addr));

  // TODO there is additional logic, that should be moved outside process function
  packets_handlers_->registerHandler(
      PriorityQueuePacketType::PQ_GetPbftBlockPacket,
      std::make_shared<GetPbftBlockPacketHandler>(peers_state_, packets_stats, syncing_state_, pbft_chain, db,
                                                  conf.network_sync_level_size, node_addr));

  const auto pbft_handler =
      std::make_shared<PbftBlockPacketHandler>(peers_state_, packets_stats, syncing_state_, syncing_handler_,
                                               pbft_chain, dag_blk_mgr, conf.network_sync_level_size, node_addr);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_PbftBlockPacket, pbft_handler);
  packets_handlers_->registerHandler(PriorityQueuePacketType::PQ_SyncedPacket, pbft_handler);

  thread_pool_.setPacketsHandlers(packets_handlers_);
}

std::string TaraxaCapability::name() const { return TARAXA_CAPABILITY_NAME; }

unsigned TaraxaCapability::version() const { return TARAXA_NET_VERSION; }

unsigned TaraxaCapability::messageCount() const { return SubprotocolPacketType::PacketCount; }

void TaraxaCapability::onConnect(weak_ptr<Session> session, u256 const &) {
  const auto node_id = session.lock()->id();

  if (syncing_state_->is_peer_malicious(node_id)) {
    if (auto session_p = session.lock()) {
      session_p->disconnect(UserReason);
    }

    LOG(log_wr_) << "Node " << node_id << " connection dropped - malicious node";
    return;
  }

  auto peer = peers_state_->addPendingPeer(node_id);
  LOG(log_nf_) << "Node " << node_id << " connected";

  // TODO: check if this cast creates a copy of shared ptr ?
  const auto &handler = packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_StatusPacket);
  auto status_packet_handler = std::static_pointer_cast<StatusPacketHandler>(handler);

  status_packet_handler->sendStatus(node_id, true);
}

void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  LOG(log_nf_) << "Node " << _nodeID << " disconnected";
  peers_state_->erasePeer(_nodeID);

  if (syncing_state_->is_pbft_syncing() && syncing_state_->syncing_peer() == _nodeID) {
    if (peers_state_->getPeersCount() > 0) {
      LOG(log_dg_) << "Restart PBFT/DAG syncing due to syncing peer disconnect.";
      restartSyncingPbft(true);
    } else {
      LOG(log_dg_) << "Stop PBFT/DAG syncing due to syncing peer disconnect and no other peers available.";
      syncing_state_->set_pbft_syncing(false);
      syncing_state_->set_dag_syncing(false);
    }
  }
}

std::string TaraxaCapability::packetTypeToString(unsigned _packetType) const {
  return convertPacketTypeToString(static_cast<SubprotocolPacketType>(_packetType));
}

void TaraxaCapability::interpretCapabilityPacket(weak_ptr<Session> session, unsigned _id, RLP const &_r) {
  auto node_id = session.lock()->id();

  // Drop any packet (except StatusPacket) that comes before the connection between nodes is initialized by sending
  // and received initial status packet
  // TODO: this logic is duplicated in PacketHandler::processPacket function...
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "Unable to process packet, host == nullptr";
    return;
  }

  auto peer = peers_state_->getPeer(node_id);
  if (!peer) {
    peer = peers_state_->getPendingPeer(node_id);
    if (!peer) {
      // It should not be possible to get here but log it just in case
      LOG(log_wr_) << "Peer missing in peers map, peer " << node_id.abridged() << " will be disconnected";
      host->disconnect(node_id, dev::p2p::UserReason);
      return;
    }
    if (_id != SubprotocolPacketType::StatusPacket) {
      LOG(log_wr_) << "Connected peer did not send status message, peer " << node_id.abridged()
                   << " will be disconnected";
      host->disconnect(node_id, dev::p2p::UserReason);
      return;
    }
  }

  // TODO: we are making a copy here for each packet bytes(toBytes()), which is pretty significant. Check why RLP does
  //       not support move semantics so we can take advantage of it...
  PriorityQueuePacketType pq_packet_type = mapSubProtocolToPriorityPacketType(static_cast<SubprotocolPacketType>(_id));
  thread_pool_.push(PacketData(pq_packet_type, packetTypeToString(_id), std::move(node_id), _r.data().toBytes()));
}

void TaraxaCapability::start() {
  thread_pool_.startProcessing();
  periodic_events_tp_.start();
}

void TaraxaCapability::stop() {
  thread_pool_.stopProcessing();
  periodic_events_tp_.stop();
}

const std::shared_ptr<PeersState> &TaraxaCapability::getPeersState() { return peers_state_; }

const std::shared_ptr<NodeStats> &TaraxaCapability::getNodeStats() { return node_stats_; }

void TaraxaCapability::restartSyncingPbft(bool force) { syncing_handler_->restartSyncingPbft(force); }

bool TaraxaCapability::pbft_syncing() const { return syncing_state_->is_pbft_syncing(); }

void TaraxaCapability::onNewBlockVerified(std::shared_ptr<DagBlock> const &blk, bool proposed) {
  std::static_pointer_cast<DagPacketsHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_NewBlockPacket))
      ->onNewBlockVerified(*blk, proposed);
}

// TODO: why not const ref ???
void TaraxaCapability::onNewTransactions(std::vector<taraxa::bytes> transactions) {
  std::static_pointer_cast<TransactionPacketHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_TransactionPacket))
      ->onNewTransactions(transactions, true);
}

void TaraxaCapability::onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block) {
  std::static_pointer_cast<NewPbftBlockPacketHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_NewPbftBlockPacket))
      ->onNewPbftBlock(*pbft_block);
}

void TaraxaCapability::onNewPbftVote(const Vote &vote) {
  std::static_pointer_cast<VotePacketsHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_PbftVotePacket))
      ->onNewPbftVote(vote);
}

void TaraxaCapability::broadcastPreviousRoundNextVotesBundle() {
  std::static_pointer_cast<VotePacketsHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_PbftVotePacket))
      ->broadcastPreviousRoundNextVotesBundle();
}

void TaraxaCapability::sendTransactions(dev::p2p::NodeID const &id, std::vector<taraxa::bytes> const &transactions) {
  std::static_pointer_cast<TransactionPacketHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_TransactionPacket))
      ->sendTransactions(id, transactions);
}

// METHODS USED IN TESTS ONLY
void TaraxaCapability::sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk) {
  std::static_pointer_cast<DagPacketsHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_NewBlockPacket))
      ->sendBlock(id, blk);
}

void TaraxaCapability::sendBlocks(dev::p2p::NodeID const &id, std::vector<std::shared_ptr<DagBlock>> blocks) {
  std::static_pointer_cast<GetBlocksPacketsHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_GetBlocksPacket))
      ->sendBlocks(id, std::move(blocks));
}

size_t TaraxaCapability::getReceivedBlocksCount() const { return test_state_->getBlocksSize(); }

size_t TaraxaCapability::getReceivedTransactionsCount() const { return test_state_->getTransactionsSize(); }

// PBFT
void TaraxaCapability::sendPbftBlock(dev::p2p::NodeID const &id, PbftBlock const &pbft_block,
                                     uint64_t pbft_chain_size) {
  std::static_pointer_cast<NewPbftBlockPacketHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_NewPbftBlockPacket))
      ->sendPbftBlock(id, pbft_block, pbft_chain_size);
}

void TaraxaCapability::sendPbftVote(dev::p2p::NodeID const &id, Vote const &vote) {
  std::static_pointer_cast<VotePacketsHandler>(
      packets_handlers_->getSpecificHandler(PriorityQueuePacketType::PQ_PbftVotePacket))
      ->sendPbftVote(id, vote);
}
// END METHODS USED IN TESTS ONLY

}  // namespace taraxa::network::tarcap
