#include "new_taraxa_capability.hpp"

#include <algorithm>

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag.hpp"
#include "network/tarcap/packets_handler/handlers/status_packet_handler.hpp"
#include "network/tarcap/packets_handler/handlers/test_packet_handler.hpp"
#include "network/tarcap/packets_handler/packets_handler.hpp"
#include "network/tarcap/packets_handler/peers_state.hpp"
#include "network/tarcap/packets_handler/syncing_state.hpp"
#include "node/full_node.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

TaraxaCapability::TaraxaCapability(std::weak_ptr<dev::p2p::Host> _host, NetworkConfig const &_conf,
                                   std::shared_ptr<DbStorage> db __attribute__((unused)),
                                   std::shared_ptr<PbftManager> pbft_mgr __attribute__((unused)),
                                   std::shared_ptr<PbftChain> pbft_chain,
                                   std::shared_ptr<VoteManager> vote_mgr __attribute__((unused)),
                                   std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr __attribute__((unused)),
                                   std::shared_ptr<DagManager> dag_mgr,
                                   std::shared_ptr<DagBlockManager> dag_blk_mgr __attribute__((unused)),
                                   std::shared_ptr<TransactionManager> trx_mgr __attribute__((unused)),
                                   addr_t const &node_addr)

    : peers_state_(std::make_shared<PeersState>(std::move(_host))),
      syncing_state_(std::make_shared<SyncingState>(peers_state_, pbft_chain, dag_mgr, dag_blk_mgr, node_addr)),
      packets_handlers_(),
      thread_pool_(10, node_addr)  // TODO: num of threads from config
//    : host_(move(_host)),
//      db_(db),
//      pbft_mgr_(pbft_mgr),
//      pbft_chain_(pbft_chain),
//      vote_mgr_(vote_mgr),
//      next_votes_mgr_(next_votes_mgr),
//      dag_mgr_(dag_mgr),
//      dag_blk_mgr_(dag_blk_mgr),
//      trx_mgr_(trx_mgr),
//      lambda_ms_min_(pbft_mgr_ ? pbft_mgr_->getPbftInitialLambda() : 2000),
//      conf_(_conf),
//      urng_(std::mt19937_64(std::random_device()())),
//      delay_rng_(std::mt19937(std::random_device()())),
//      random_dist_(std::uniform_int_distribution<std::mt19937::result_type>(90, 110)) {
//  LOG_OBJECTS_CREATE("TARCAP");
//  LOG_OBJECTS_CREATE_SUB("PBFTSYNC", pbft_sync);
//  LOG_OBJECTS_CREATE_SUB("DAGSYNC", dag_sync);
//  LOG_OBJECTS_CREATE_SUB("NEXTVOTESSYNC", next_votes_sync);
//  LOG_OBJECTS_CREATE_SUB("DAGPRP", dag_prp);
//  LOG_OBJECTS_CREATE_SUB("TRXPRP", trx_prp);
//  LOG_OBJECTS_CREATE_SUB("PBFTPRP", pbft_prp);
//  LOG_OBJECTS_CREATE_SUB("VOTEPRP", vote_prp);
//  LOG_OBJECTS_CREATE_SUB("NETPER", net_per);
//  LOG_OBJECTS_CREATE_SUB("SUMMARY", summary);
{
  // Register all packet handlers
  packets_handlers_->registerHandler(SubprotocolPacketType::TestPacket,
                                     std::make_shared<TestPacketHandler>(peers_state_, node_addr));
  packets_handlers_->registerHandler(SubprotocolPacketType::StatusPacket,
                                     std::make_shared<StatusPacketHandler>(peers_state_, syncing_state_, pbft_chain,
                                                                           dag_mgr, _conf.network_id, node_addr));

  thread_pool_.setPacketsHandlers(packets_handlers_);

  LOG_OBJECTS_CREATE("TARCAP");

  // TODO use this
  //
  //  if (conf_.network_transaction_interval > 0) {
  //    tp_.post(conf_.network_transaction_interval, [this] { sendTransactions(); });
  //  }
  //  check_alive_interval_ = 6 * lambda_ms_min_;
  //  tp_.post(check_alive_interval_, [this] { checkLiveness(); });
  //  if (conf_.network_performance_log_interval > 0) {
  //    tp_.post(conf_.network_performance_log_interval, [this] { logPacketsStats(); });
  //  }
  //
  //  summary_interval_ms_ = 5 * 6 * lambda_ms_min_;
  //  tp_.post(summary_interval_ms_, [this] { logNodeStats(); });
}

std::string TaraxaCapability::name() const {
  // TODO: replace hardcoded "taraxa" by some common function that is also used in sealAndSend function
  return "taraxa";
}

unsigned TaraxaCapability::version() const { return TARAXA_NET_VERSION; }

unsigned TaraxaCapability::messageCount() const { return packets_types_count(); }

void TaraxaCapability::onConnect(weak_ptr<Session> session, u256 const &) {
  const auto node_id = session.lock()->id();

  if (syncing_state_->is_peer_malicious(node_id)) {
    if (auto session_p = session.lock()) {
      session_p->disconnect(UserReason);
    }

    LOG(log_wr_) << "Node " << node_id << " dropped as is marked malicious";
    return;
  }

  auto peer = peers_state_->addPendingPeer(node_id);

  // TODO: check if this cast creates a copy of shared ptr ?
  const auto &handler = packets_handlers_->getSpecificHandler(SubprotocolPacketType::StatusPacket);
  auto status_packet_handler = std::static_pointer_cast<StatusPacketHandler>(handler);

  status_packet_handler->sendStatus(node_id, true);
  LOG(log_nf_) << "Node " << node_id << " connected";
}

void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  LOG(log_nf_) << "Node " << _nodeID << " disconnected";

  peers_state_->erasePeer(_nodeID);
  syncing_state_->processDisconnect(_nodeID);
}

std::string TaraxaCapability::packetTypeToString(unsigned _packetType) const {
  switch (_packetType) {
    case StatusPacket:
      return "StatusPacket";
    case NewBlockPacket:
      return "NewBlockPacket";
    case NewBlockHashPacket:
      return "NewBlockHashPacket";
    case GetNewBlockPacket:
      return "GetNewBlockPacket";
    case GetBlocksPacket:
      return "GetBlocksPacket";
    case BlocksPacket:
      return "BlocksPacket";
    case TransactionPacket:
      return "TransactionPacket";
    case TestPacket:
      return "TestPacket";
    case PbftVotePacket:
      return "PbftVotePacket";
    case GetPbftNextVotes:
      return "GetPbftNextVotes";
    case PbftNextVotesPacket:
      return "PbftNextVotesPacket";
    case NewPbftBlockPacket:
      return "NewPbftBlockPacket";
    case GetPbftBlockPacket:
      return "GetPbftBlockPacket";
    case PbftBlockPacket:
      return "PbftBlockPacket";
    case SyncedPacket:
      return "SyncedPacket";
    case SyncedResponsePacket:
      return "SyncedResponsePacket";
  }
  return "unknown packet type: " + std::to_string(_packetType);
}

void TaraxaCapability::interpretCapabilityPacket(weak_ptr<Session> session, unsigned _id, RLP const &_r) {
  auto node_id = session.lock()->id();

  // Drop any packet (except StatusPacket) that comes before the connection between nodes is initialized by sending
  // and received initial status packet
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
      LOG(log_er_) << "Peer missing in peers map, peer " << node_id.abridged() << " will be disconnected";
      host->disconnect(node_id, dev::p2p::UserReason);
      return;
    }
    if (_id != SubprotocolPacketType::StatusPacket) {
      LOG(log_er_) << "Connected peer did not send status message, peer " << node_id.abridged()
                   << " will be disconnected";
      host->disconnect(node_id, dev::p2p::UserReason);
      return;
    }
  }

  // TODO: we are making a copy here for each packet bytes(toBytes()), which is pretty significant. Check why RLP does
  //       not support move semantics so we can take advantage of it...
  thread_pool_.push(PacketData(static_cast<SubprotocolPacketType>(_id), std::move(node_id), _r.data().toBytes()));
}

void TaraxaCapability::startProcessingPackets() { thread_pool_.startProcessing(); }

// TODO: delete me
void TaraxaCapability::pushData(unsigned _id, RLP const &_r) {
  thread_pool_.push(PacketData(static_cast<SubprotocolPacketType>(_id), dev::p2p::NodeID(), _r.data().toBytes()));
}

}  // namespace taraxa::network::tarcap
