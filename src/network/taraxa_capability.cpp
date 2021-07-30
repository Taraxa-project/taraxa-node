#include "taraxa_capability.hpp"

#include <algorithm>

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag.hpp"
#include "node/full_node.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa {

TaraxaCapability::TaraxaCapability(weak_ptr<Host> _host, NetworkConfig const &_conf, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                   std::shared_ptr<VoteManager> vote_mgr,
                                   std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr,
                                   std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                   std::shared_ptr<TransactionManager> trx_mgr, addr_t const &node_addr)
    : host_(move(_host)),
      peers_state_(node_addr),
      db_(db),
      pbft_mgr_(pbft_mgr),
      pbft_chain_(pbft_chain),
      vote_mgr_(vote_mgr),
      next_votes_mgr_(next_votes_mgr),
      dag_mgr_(dag_mgr),
      dag_blk_mgr_(dag_blk_mgr),
      trx_mgr_(trx_mgr),
      lambda_ms_min_(pbft_mgr_ ? pbft_mgr_->getPbftInitialLambda() : 2000),
      conf_(_conf),
      delay_rng_(std::mt19937(std::random_device()())),
      random_dist_(std::uniform_int_distribution<std::mt19937::result_type>(90, 110)) {
  LOG_OBJECTS_CREATE("TARCAP");
  LOG_OBJECTS_CREATE_SUB("PBFTSYNC", pbft_sync);
  LOG_OBJECTS_CREATE_SUB("DAGSYNC", dag_sync);
  LOG_OBJECTS_CREATE_SUB("NEXTVOTESSYNC", next_votes_sync);
  LOG_OBJECTS_CREATE_SUB("DAGPRP", dag_prp);
  LOG_OBJECTS_CREATE_SUB("TRXPRP", trx_prp);
  LOG_OBJECTS_CREATE_SUB("PBFTPRP", pbft_prp);
  LOG_OBJECTS_CREATE_SUB("VOTEPRP", vote_prp);
  LOG_OBJECTS_CREATE_SUB("NETPER", net_per);
  LOG_OBJECTS_CREATE_SUB("SUMMARY", summary);
  auto host = host_.lock();
  assert(host);
  node_id_ = host->id();
  if (conf_.network_transaction_interval > 0) {
    tp_.post(conf_.network_transaction_interval, [this] { sendTransactions(); });
  }
  check_alive_interval_ = 6 * lambda_ms_min_;
  tp_.post(check_alive_interval_, [this] { checkLiveness(); });
  if (conf_.network_performance_log_interval > 0) {
    tp_.post(conf_.network_performance_log_interval, [this] { logPacketsStats(); });
  }

  summary_interval_ms_ = 5 * 6 * lambda_ms_min_;
  tp_.post(summary_interval_ms_, [this] { logNodeStats(); });
}

void TaraxaCapability::syncPeerPbft(unsigned long height_to_sync) {
  const auto node_id = syncing_state_.syncing_peer();
  LOG(log_nf_pbft_sync_) << "Sync peer node " << node_id << " from pbft chain height " << height_to_sync;
  requestPbftBlocks(node_id, height_to_sync);
}

bool TaraxaCapability::sealAndSend(NodeID const &nodeID, unsigned packet_type, RLPStream rlp) {
  auto host = host_.lock();
  if (!host) {
    LOG(log_er_) << "sealAndSend failed to obtain host";
    return false;
  }

  auto peer = peers_state_.getPeer(nodeID);
  if (!peer) {
    peer = peers_state_.getPendingPeer(nodeID);
    if (peer) {
      if (packet_type != StatusPacket) {
        LOG(log_er_) << "sealAndSend failed initial status check, peer " << nodeID.abridged()
                     << " will be disconnected";
        host->disconnect(nodeID, p2p::UserReason);
        return false;
      }
    } else {
      LOG(log_wr_) << "sealAndSend failed to find peer";
      return false;
    }
  }

  auto packet_size = rlp.out().size();

  // This situation should never happen - packets bigger than 16MB cannot be sent due to networking layer limitations.
  // If we are trying to send packets bigger than that, it should be split to multiple packets
  // or handled in some other way in high level code - e.g. function that creates such packet and calls sealAndSend
  if (packet_size > MAX_PACKET_SIZE) {
    LOG(log_er_) << "Trying to send packet bigger than PACKET_MAX_SIZE(" << MAX_PACKET_SIZE << ") - rejected !"
                 << " Packet type: " << packetTypeToString(packet_type) << ", size: " << packet_size
                 << ", receiver: " << nodeID.abridged();
    return false;
  }

  auto begin = std::chrono::steady_clock::now();
  host->send(nodeID, name(), packet_type, move(rlp.invalidate()), [=] {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    tp_.post([=] {  // schedule it out of the socket session thread. although maybe it's an overkill
      PacketStats packet_stats{nodeID, packet_size, false, duration};
      sent_packets_stats_.addPacket(packetTypeToString(packet_type), packet_stats);
      LOG(log_dg_net_per_) << "(\"" << node_id_ << "\") sent " << packetTypeToString(packet_type) << " packet to (\""
                           << nodeID << "\"). Stats: " << packet_stats;
    });
  });

  return true;
}

std::pair<bool, std::vector<blk_hash_t>> TaraxaCapability::checkDagBlockValidation(DagBlock const &block) {
  std::vector<blk_hash_t> missing_blks;

  if (dag_blk_mgr_->getDagBlock(block.getHash())) {
    // The DAG block exist
    return std::make_pair(true, missing_blks);
  }

  level_t expected_level = 0;
  for (auto const &tip : block.getTips()) {
    auto tip_block = dag_blk_mgr_->getDagBlock(tip);
    if (!tip_block) {
      LOG(log_er_dag_sync_) << "Block " << block.getHash().toString() << " has a missing tip " << tip.toString();
      missing_blks.push_back(tip);
    } else {
      expected_level = std::max(tip_block->getLevel(), expected_level);
    }
  }

  auto pivot = block.getPivot();
  auto pivot_block = dag_blk_mgr_->getDagBlock(pivot);
  if (!pivot_block) {
    LOG(log_er_dag_sync_) << "Block " << block.getHash().toString() << " has a missing pivot " << pivot.toString();
    missing_blks.push_back(pivot);
  }

  if (missing_blks.size()) return std::make_pair(false, missing_blks);

  expected_level = std::max(pivot_block->getLevel(), expected_level);
  expected_level++;
  if (expected_level != block.getLevel()) {
    LOG(log_er_dag_sync_) << "Invalid block level " << block.getLevel() << " for block " << block.getHash()
                          << " . Expected level " << expected_level;
    return std::make_pair(false, missing_blks);
  }

  return std::make_pair(true, missing_blks);
}

void TaraxaCapability::requestBlocks(const NodeID &_nodeID, std::vector<blk_hash_t> const &blocks,
                                     GetBlocksPacketRequestType mode) {
  LOG(log_nf_dag_sync_) << "Sending GetBlocksPacket";
  RLPStream s(blocks.size() + 1);  // Mode + block itself
  s << mode;                       // Send mode first
  for (auto blk : blocks) {
    s << blk;
  }
  sealAndSend(_nodeID, GetBlocksPacket, s);
}

void TaraxaCapability::onConnect(weak_ptr<Session> session, u256 const &) {
  auto _nodeID = session.lock()->id();

  if (syncing_state_.is_peer_malicious(_nodeID)) {
    if (auto session_p = session.lock()) {
      session_p->disconnect(UserReason);
    }
    LOG(log_wr_) << "Node " << _nodeID << " dropped as is marked malicious";
    return;
  }

  LOG(log_nf_) << "Node " << _nodeID << " connected";
  auto peer = peers_state_.addPendingPeer(_nodeID);
  tp_.post([=] {
    cnt_received_messages_[_nodeID] = 0;
    test_sums_[_nodeID] = 0;
    sendStatus(_nodeID, true);
  });
}

uint64_t TaraxaCapability::getSimulatedNetworkDelay(const RLP &packet_rlp, const NodeID &nodeID) {
  // RLP contains memory it does not own so deep copy of bytes is needed
  dev::bytes rBytes = packet_rlp.data().toBytes();
  int messageSize = rBytes.size() * 8;
  unsigned int dist = *((int *)node_id_.data()) ^ *((int *)nodeID.data());
  unsigned int delay = dist % conf_.network_simulated_delay;

  auto bandwidth = conf_.network_bandwidth ? conf_.network_bandwidth : 40;
  unsigned int bandwidth_delay = messageSize / (bandwidth * 1000);  // in ms

  // Random component up to +-10%
  int random_component = random_dist_(delay_rng_);

  return (delay + bandwidth_delay) * random_component / 100;
}

void TaraxaCapability::interpretCapabilityPacket(weak_ptr<Session> session, unsigned _id, RLP const &_r) {
  auto const &_nodeID = session.lock()->id();
  // Delay is used only when we want to simulate some network delay
  uint64_t delay = conf_.network_simulated_delay ? getSimulatedNetworkDelay(_r, _nodeID) : 0;

  tp_.post(delay, [=, _r_copy = _r.data().toBytes()] {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    RLP r(_r_copy);
    PacketStats packet_stats{_nodeID, r.actualSize(), false, std::chrono::microseconds(0)};
    try {
      interpretCapabilityPacketImpl(_nodeID, _id, r, packet_stats);
    } catch (...) {
      handle_read_exception(session, _id);
    }
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    packet_stats.total_duration_ = duration;
    received_packets_stats_.addPacket(packetTypeToString(_id), packet_stats);

    LOG(log_dg_net_per_) << "(\"" << node_id_ << "\") received " << packetTypeToString(_id) << " packet from (\""
                         << _nodeID << "\"). Stats: " << packet_stats;
  });
}

unsigned TaraxaCapability::version() const { return TARAXA_NET_VERSION; }

void TaraxaCapability::interpretCapabilityPacketImpl(NodeID const &_nodeID, unsigned _id, RLP const &_r,
                                                     PacketStats &packet_stats) {
  auto host = host_.lock();
  if (!host) {
    return;
  }
  auto peer = peers_state_.getPeer(_nodeID);
  if (!peer) {
    peer = peers_state_.getPendingPeer(_nodeID);
    if (!peer) {
      // It should not be possible to get here but log it just in case
      LOG(log_wr_) << "Peer missing in peers map, peer " << _nodeID.abridged() << " will be disconnected";
      host->disconnect(_nodeID, p2p::UserReason);
      return;
    }
    if (_id != StatusPacket) {
      LOG(log_wr_) << "Connected peer did not send status message, peer " << _nodeID.abridged()
                   << " will be disconnected";
      host->disconnect(_nodeID, p2p::UserReason);
      return;
    }
  }

  // Any packet means that we are comunicating so let's not disconnect
  // e.g. we could send a lot of data and status packet can be in send queue
  peer->setAlive();

  switch (_id) {
    case SyncedPacket: {
      LOG(log_dg_dag_sync_) << "Received synced message from " << _nodeID;
      peer->syncing_ = false;
      // peer->clearAllKnownBlocksAndTransactions();
      break;
    }
    case StatusPacket: {
      bool initial_status = _r.itemCount() == INITIAL_STATUS_PACKET_ITEM_COUNT;

      if (initial_status) {
        auto it = _r.begin();
        auto const network_id = (*it++).toInt<uint64_t>();
        auto const peer_dag_level = (*it++).toPositiveInt64();
        auto const genesis_hash = blk_hash_t(*it++);
        auto const peer_pbft_chain_size = (*it++).toPositiveInt64();
        auto const peer_syncing = (*it++).toInt();
        auto const peer_pbft_round = (*it++).toPositiveInt64();
        auto const peer_pbft_previous_round_next_votes_size = (*it++).toInt<unsigned>();
        auto const node_major_version = (*it++).toInt<unsigned>();
        auto const node_minor_version = (*it++).toInt<unsigned>();
        auto const node_patch_version = (*it++).toInt<unsigned>();

        // We need logic when some different node versions might still be compatible
        if (node_major_version != TARAXA_MAJOR_VERSION || node_minor_version != TARAXA_MINOR_VERSION) {
          LOG(log_er_) << "Incorrect node version: "
                       << getFormattedVersion({node_major_version, node_minor_version, node_patch_version})
                       << ", our node version" << TARAXA_VERSION << ", host " << _nodeID.abridged()
                       << " will be disconnected";
          host->disconnect(_nodeID, p2p::UserReason);
          break;
        }

        if (network_id != conf_.network_id) {
          LOG(log_er_) << "Incorrect network id " << network_id << ", host " << _nodeID.abridged()
                       << " will be disconnected";
          host->disconnect(_nodeID, p2p::UserReason);
          break;
        }

        if (genesis_hash != dag_mgr_->get_genesis()) {
          LOG(log_er_) << "Incorrect genesis hash " << genesis_hash << ", host " << _nodeID.abridged()
                       << " will be disconnected";
          host->disconnect(_nodeID, p2p::UserReason);
          break;
        }

        peer->dag_level_ = peer_dag_level;
        peer->pbft_chain_size_ = peer_pbft_chain_size;
        peer->syncing_ = peer_syncing;
        peer->pbft_round_ = peer_pbft_round;
        peer->pbft_previous_round_next_votes_size_ = peer_pbft_previous_round_next_votes_size;

        setPeerAsReadyToSendMessages(_nodeID, peer);

        LOG(log_dg_) << "Received initial status message from " << _nodeID << ", network id " << network_id
                     << ", peer DAG max level " << peer->dag_level_ << ", genesis " << genesis_hash
                     << ", peer pbft chain size " << peer->pbft_chain_size_ << ", peer syncing " << std::boolalpha
                     << peer->syncing_ << ", peer pbft round " << peer->pbft_round_
                     << ", peer pbft previous round next votes size " << peer->pbft_previous_round_next_votes_size_
                     << ", node major version" << node_major_version << ", node minor version" << node_minor_version;

      } else {
        auto it = _r.begin();
        peer->dag_level_ = (*it++).toPositiveInt64();
        peer->pbft_chain_size_ = (*it++).toPositiveInt64();
        peer->syncing_ = (*it++).toInt();
        peer->pbft_round_ = (*it++).toPositiveInt64();
        peer->pbft_previous_round_next_votes_size_ = (*it++).toInt<unsigned>();

        LOG(log_dg_) << "Received status message from " << _nodeID << ", peer DAG max level " << peer->dag_level_
                     << ", peer pbft chain size " << peer->pbft_chain_size_ << ", peer syncing " << std::boolalpha
                     << peer->syncing_ << ", peer pbft round " << peer->pbft_round_
                     << ", peer pbft previous round next votes size " << peer->pbft_previous_round_next_votes_size_;
      }

      LOG(log_dg_dag_sync_) << "Received status message from " << _nodeID << " peer DAG max level:" << peer->dag_level_;
      LOG(log_dg_pbft_sync_) << "Received status message from " << _nodeID << ", peer sycning: " << std::boolalpha
                             << peer->syncing_ << ", peer PBFT chain size:" << peer->pbft_chain_size_;
      LOG(log_dg_next_votes_sync_) << "Received status message from " << _nodeID << ", PBFT round " << peer->pbft_round_
                                   << ", peer PBFT previous round next votes size "
                                   << peer->pbft_previous_round_next_votes_size_;

      // If we are still syncing - do not trigger new syncing
      if (syncing_state_.is_pbft_syncing() && syncing_state_.is_actively_syncing()) {
        break;
      }

      // TODO: Address the CONCERN that it isn't NECESSARY to sync here
      // and by syncing here we open node up to attack of sending bogus
      // status.  We also have nothing to punish a node failing to send
      // sync info.
      auto pbft_synced_period = pbft_chain_->pbftSyncingPeriod();
      if (pbft_synced_period + 1 < peer->pbft_chain_size_) {
        LOG(log_nf_) << "Restart PBFT chain syncing. Own synced PBFT at period " << pbft_synced_period
                     << ", peer PBFT chain size " << peer->pbft_chain_size_;
        if (pbft_synced_period + 5 < peer->pbft_chain_size_) {
          restartSyncingPbft(true);
        } else {
          restartSyncingPbft(false);
        }
      }

      auto pbft_current_round = pbft_mgr_->getPbftRound();
      auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();
      if (pbft_current_round < peer->pbft_round_ ||
          (pbft_current_round == peer->pbft_round_ &&
           pbft_previous_round_next_votes_size < peer->pbft_previous_round_next_votes_size_)) {
        syncPbftNextVotes(pbft_current_round, pbft_previous_round_next_votes_size);
      }

      break;
    }
    // Means a new block is proposed, full block body and all transaction
    // are received.
    case NewBlockPacket: {
      // Ignore new block packets when syncing
      if (syncing_state_.is_syncing()) break;
      DagBlock block(_r[0].data().toBytes());

      if (dag_blk_mgr_) {
        if (dag_blk_mgr_->isBlockKnown(block.getHash())) {
          LOG(log_dg_dag_prp_) << "Received NewBlock " << block.getHash().toString() << "that is already known";
          break;
        }
        if (auto status = checkDagBlockValidation(block); !status.first) {
          LOG(log_wr_dag_prp_) << "Received NewBlock " << block.getHash().toString() << " missing pivot or/and tips";
          status.second.push_back(block.getHash());
          requestBlocks(_nodeID, status.second);
          break;
        }
      }

      packet_stats.is_unique_ = true;

      auto transactionsCount = _r.itemCount() - 1;
      LOG(log_dg_dag_prp_) << "Received NewBlockPacket " << transactionsCount;

      std::vector<Transaction> newTransactions;
      for (size_t iTransaction = 1; iTransaction < transactionsCount + 1; iTransaction++) {
        Transaction transaction(_r[iTransaction].data().toBytes());
        newTransactions.push_back(transaction);
        peer->markTransactionAsKnown(transaction.getHash());
      }

      peer->markBlockAsKnown(block.getHash());
      if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();
      onNewBlockReceived(block, newTransactions);
      break;
    }

    case NewBlockHashPacket: {
      blk_hash_t hash(_r[0]);
      LOG(log_dg_dag_prp_) << "Received NewBlockHashPacket " << hash.toString();
      peer->markBlockAsKnown(hash);
      if (dag_blk_mgr_) {
        if (!dag_blk_mgr_->isBlockKnown(hash) && block_requestes_set_.count(hash) == 0) {
          packet_stats.is_unique_ = true;
          block_requestes_set_.insert(hash);
          requestBlock(_nodeID, hash);
        }
      } else if (test_blocks_.find(hash) == test_blocks_.end() && block_requestes_set_.count(hash) == 0) {
        block_requestes_set_.insert(hash);
        requestBlock(_nodeID, hash);
      }
      break;
    }
    case GetNewBlockPacket: {
      blk_hash_t hash(_r[0]);
      peer->markBlockAsKnown(hash);
      LOG(log_dg_dag_prp_) << "Received GetNewBlockPacket" << hash.toString();

      if (dag_blk_mgr_) {
        auto block = db_->getDagBlock(hash);
        if (block) {
          sendBlock(_nodeID, *block);
        } else
          LOG(log_nf_dag_prp_) << "NO NEW PACKET: " << hash.toString();
      } else if (test_blocks_.find(hash) != test_blocks_.end()) {
        sendBlock(_nodeID, test_blocks_[hash]);
      }
      break;
    }
    case GetBlocksPacket: {
      std::unordered_set<blk_hash_t> blocks_hashes;
      std::vector<std::shared_ptr<DagBlock>> dag_blocks;
      auto it = _r.begin();
      auto mode = static_cast<GetBlocksPacketRequestType>((*it++).toInt<unsigned>());

      if (mode == MissingHashes)
        LOG(log_dg_dag_sync_) << "Received GetBlocksPacket with " << _r.itemCount() - 1 << " missing blocks";
      else if (mode == KnownHashes)
        LOG(log_dg_dag_sync_) << "Received GetBlocksPacket with " << _r.itemCount() - 1 << " known blocks";

      for (; it != _r.end(); ++it) {
        blocks_hashes.emplace(*it);
      }

      const auto &blocks = dag_mgr_->getNonFinalizedBlocks();
      for (auto &level_blocks : blocks) {
        for (auto &block : level_blocks.second) {
          auto hash = block;
          if (mode == MissingHashes) {
            if (blocks_hashes.count(hash) == 1) {
              if (auto blk = db_->getDagBlock(hash); blk) {
                dag_blocks.emplace_back(blk);
              } else {
                LOG(log_er_dag_sync_) << "NonFinalizedBlock " << hash << " not in DB";
                assert(false);
              }
            }
          } else if (mode == KnownHashes) {
            if (blocks_hashes.count(hash) == 0) {
              if (auto blk = db_->getDagBlock(hash); blk) {
                dag_blocks.emplace_back(blk);
              } else {
                LOG(log_er_dag_sync_) << "NonFinalizedBlock " << hash << " not in DB";
                assert(false);
              }
            }
          }
        }
      }

      // This means that someone requested more hashes that we actually have -> do not send anything
      if (mode == MissingHashes && dag_blocks.size() != blocks_hashes.size()) {
        LOG(log_nf_dag_sync_) << "Node " << _nodeID << " requested unknown DAG block";
        break;
      }
      sendBlocks(_nodeID, dag_blocks);
      break;
    }
    case BlocksPacket: {
      std::string received_dag_blocks_str;
      auto it = _r.begin();
      bool is_final_sync_packet = (*it++).toInt<unsigned>();

      for (; it != _r.end();) {
        DagBlock block(*it++);
        peer->markBlockAsKnown(block.getHash());

        std::vector<Transaction> newTransactions;
        for (size_t i = 0; i < block.getTrxs().size(); i++) {
          Transaction transaction(*it++);
          newTransactions.push_back(transaction);
          peer->markTransactionAsKnown(transaction.getHash());
        }

        received_dag_blocks_str += block.getHash().toString() + " ";

        auto status = checkDagBlockValidation(block);
        if (!status.first) {
          LOG(log_wr_dag_sync_) << "DagBlockValidation failed " << status.second;
          status.second.push_back(block.getHash());
          requestBlocks(_nodeID, status.second);
          continue;
        }

        LOG(log_dg_dag_sync_) << "Storing block " << block.getHash().toString() << " with " << newTransactions.size()
                              << " transactions";
        if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();
        dag_blk_mgr_->insertBroadcastedBlockWithTransactions(block, newTransactions);
      }

      if (is_final_sync_packet) {
        syncing_state_.set_dag_syncing(false);
        LOG(log_nf_dag_sync_) << "Received final DagBlocksSyncPacket with blocks: " << received_dag_blocks_str;
      } else {
        syncing_state_.set_last_sync_packet_time();
        LOG(log_nf_dag_sync_) << "Received partial DagBlocksSyncPacket with blocks: " << received_dag_blocks_str;
      }

      break;
    }

    case TransactionPacket: {
      std::string receivedTransactions;
      std::vector<taraxa::bytes> transactions;
      auto transactionCount = _r.itemCount();
      for (size_t iTransaction = 0; iTransaction < transactionCount; iTransaction++) {
        Transaction transaction(_r[iTransaction].data().toBytes());
        receivedTransactions += transaction.getHash().toString() + " ";
        peer->markTransactionAsKnown(transaction.getHash());
        transactions.emplace_back(_r[iTransaction].data().toBytes());
      }
      if (transactionCount > 0) {
        LOG(log_dg_trx_prp_) << "Received TransactionPacket with " << _r.itemCount() << " transactions";
        LOG(log_tr_trx_prp_) << "Received TransactionPacket with " << _r.itemCount()
                             << " transactions:" << receivedTransactions.c_str();

        onNewTransactions(transactions, true);
      }
      break;
    }

    case PbftVotePacket: {
      LOG(log_dg_vote_prp_) << "In PbftVotePacket";

      Vote vote(_r[0].toBytes(), false);
      auto vote_hash = vote.getHash();
      LOG(log_dg_vote_prp_) << "Received PBFT vote " << vote_hash;

      auto pbft_round = pbft_mgr_->getPbftRound();
      auto vote_round = vote.getRound();

      if (vote_round >= pbft_round) {
        if (!vote_mgr_->voteInUnverifiedMap(vote_round, vote_hash) &&
            !vote_mgr_->voteInVerifiedMap(vote_round, vote_hash)) {
          db_->saveUnverifiedVote(vote);
          vote_mgr_->addUnverifiedVote(vote);
          packet_stats.is_unique_ = true;

          peer->markVoteAsKnown(vote_hash);

          onNewPbftVote(vote);
        }
      }

      break;
    }

    case GetPbftNextVotes: {
      LOG(log_dg_next_votes_sync_) << "Received GetPbftNextVotes request";

      uint64_t peer_pbft_round = _r[0].toPositiveInt64();
      size_t peer_pbft_previous_round_next_votes_size = _r[1].toInt<unsigned>();
      uint64_t pbft_round = pbft_mgr_->getPbftRound();
      size_t pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();

      if (pbft_round > peer_pbft_round ||
          (pbft_round == peer_pbft_round &&
           pbft_previous_round_next_votes_size > peer_pbft_previous_round_next_votes_size)) {
        LOG(log_dg_next_votes_sync_) << "Current PBFT round is " << pbft_round << " previous round next votes size "
                                     << pbft_previous_round_next_votes_size << ", and peer PBFT round is "
                                     << peer_pbft_round << " previous round next votes size "
                                     << peer_pbft_previous_round_next_votes_size
                                     << ". Will send out bundle of next votes";

        auto next_votes_bundle = next_votes_mgr_->getNextVotes();
        std::vector<Vote> send_next_votes_bundle;
        for (auto const &v : next_votes_bundle) {
          if (!peer->isVoteKnown(v.getHash())) {
            send_next_votes_bundle.emplace_back(v);
          }
        }
        sendPbftNextVotes(_nodeID, send_next_votes_bundle);
      }

      break;
    }
    case PbftNextVotesPacket: {
      auto next_votes_count = _r.itemCount();
      if (next_votes_count == 0) {
        LOG(log_er_next_votes_sync_) << "Receive 0 next votes from peer " << _nodeID
                                     << ". The peer may be a malicous player, will be disconnected";
        host->disconnect(_nodeID, p2p::UserReason);

        break;
      }

      auto pbft_current_round = pbft_mgr_->getPbftRound();
      Vote vote(_r[0].data().toBytes());
      auto peer_pbft_round = vote.getRound() + 1;
      std::vector<Vote> next_votes;
      for (size_t i = 0; i < next_votes_count; i++) {
        Vote next_vote(_r[i].data().toBytes());
        if (next_vote.getRound() != peer_pbft_round - 1) {
          LOG(log_er_next_votes_sync_) << "Received next votes bundle with unmatched rounds from " << _nodeID
                                       << ". The peer may be a malicous player, will be disconnected";
          host->disconnect(_nodeID, p2p::UserReason);

          break;
        }
        auto next_vote_hash = next_vote.getHash();
        LOG(log_nf_next_votes_sync_) << "Received PBFT next vote " << next_vote_hash;
        peer->markVoteAsKnown(next_vote_hash);
        next_votes.emplace_back(next_vote);
      }
      LOG(log_nf_next_votes_sync_) << "Received " << next_votes_count << " next votes from peer " << _nodeID
                                   << " node current round " << pbft_current_round << ", peer pbft round "
                                   << peer_pbft_round;

      if (pbft_current_round < peer_pbft_round) {
        // Add into votes unverified queue
        for (auto const &vote : next_votes) {
          auto vote_hash = vote.getHash();
          auto vote_round = vote.getRound();
          if (!vote_mgr_->voteInUnverifiedMap(vote_round, vote_hash) &&
              !vote_mgr_->voteInVerifiedMap(vote_round, vote_hash)) {
            // vote round >= PBFT round
            db_->saveUnverifiedVote(vote);
            vote_mgr_->addUnverifiedVote(vote);
            packet_stats.is_unique_ = true;
            onNewPbftVote(vote);
          }
        }
      } else if (pbft_current_round == peer_pbft_round) {
        // Update previous round next votes
        auto pbft_2t_plus_1 = db_->getPbft2TPlus1(pbft_current_round - 1);
        if (pbft_2t_plus_1) {
          // Update our previous round next vote bundles...
          next_votes_mgr_->updateWithSyncedVotes(next_votes, pbft_2t_plus_1);
          // Pass them on to our peers...
          auto updated_next_votes_size = next_votes_mgr_->getNextVotesSize();
          for (auto const &peerToShareTo : peers_state_.getAllPeers()) {
            // Do not send votes right back to same peer...
            if (peerToShareTo.first == _nodeID) continue;
            // Do not send votes to nodes that already have as many bundles as we do...
            if (peerToShareTo.second->pbft_previous_round_next_votes_size_ >= updated_next_votes_size) continue;
            // Nodes may vote at different values at previous round, so need less or equal
            if (!peerToShareTo.second->syncing_ && peerToShareTo.second->pbft_round_ <= pbft_current_round) {
              std::vector<Vote> send_next_votes_bundle;
              for (auto const &v : next_votes) {
                if (!peerToShareTo.second->isVoteKnown(v.getHash())) {
                  send_next_votes_bundle.emplace_back(v);
                }
              }
              sendPbftNextVotes(peerToShareTo.first, send_next_votes_bundle);
            }
          }
        } else {
          LOG(log_er_) << "Cannot get PBFT 2t+1 in PBFT round " << pbft_current_round - 1;
        }
      }
      break;
    }

    case GetPbftBlockPacket: {
      LOG(log_dg_pbft_sync_) << "Received GetPbftBlockPacket Block";

      size_t height_to_sync = _r[0].toInt();
      // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
      size_t my_chain_size = pbft_chain_->getPbftChainSize();
      size_t blocks_to_transfer = 0;
      if (my_chain_size >= height_to_sync) {
        blocks_to_transfer =
            std::min((uint64_t)conf_.network_sync_level_size, (uint64_t)(my_chain_size - (height_to_sync - 1)));
      }
      LOG(log_dg_pbft_sync_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << _nodeID;

      // Stop syncing if too many sync requests or if the rest of the node is busy
      if (blocks_to_transfer > 0) {
        if (syncing_tp_.num_pending_tasks() >= MAX_SYNCING_NODES) {
          LOG(log_wr_pbft_sync_) << "Node is already serving max syncing nodes, host " << _nodeID.abridged()
                                 << " will be disconnected";
          host->disconnect(_nodeID, p2p::UserReason);
          break;
        }
      }
      // If blocks_to_transfer is 0, send peer empty PBFT blocks for talking to peer syncing has completed
      syncing_tp_.post([host, _nodeID, height_to_sync, blocks_to_transfer, this] {
        bool send_blocks = true;
        if (blocks_to_transfer > 0) {
          uint32_t total_wait_time = 0;
          while (tp_.num_pending_tasks() > MAX_NETWORK_QUEUE_TO_DROP_SYNCING) {
            uint32_t delay_time = MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS / 10;
            thisThreadSleepForMilliSeconds(delay_time);
            total_wait_time += delay_time;
            if (total_wait_time >= MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS) {
              LOG(log_er_pbft_sync_) << "Node is busy with " << tp_.num_pending_tasks() << " network packets. Host "
                                     << _nodeID.abridged() << " will be disconnected";
              host->disconnect(_nodeID, p2p::UserReason);
              send_blocks = false;
              break;
            }
          }
        }
        if (send_blocks) sendPbftBlocks(_nodeID, height_to_sync, blocks_to_transfer);
      });
      break;
    }

    // no cert vote needed (propose block)
    case NewPbftBlockPacket: {
      LOG(log_dg_pbft_prp_) << "In NewPbftBlockPacket";

      auto pbft_block = std::make_shared<PbftBlock>(_r[0]);
      uint64_t peer_pbft_chain_size = _r[1].toInt();
      LOG(log_dg_pbft_prp_) << "Receive proposed PBFT Block " << pbft_block
                            << ", Peer PBFT Chain size: " << peer_pbft_chain_size;

      peer->markPbftBlockAsKnown(pbft_block->getBlockHash());
      if (peer_pbft_chain_size > peer->pbft_chain_size_) {
        peer->pbft_chain_size_ = peer_pbft_chain_size;
      }

      auto pbft_synced_period = pbft_chain_->pbftSyncingPeriod();
      if (pbft_synced_period >= pbft_block->getPeriod()) {
        LOG(log_dg_pbft_prp_) << "Drop it! Synced PBFT block at period " << pbft_block->getPeriod()
                              << ", own PBFT chain has synced at period " << pbft_synced_period;
        return;
      }

      if (!pbft_chain_->findUnverifiedPbftBlock(pbft_block->getBlockHash())) {
        packet_stats.is_unique_ = true;
        pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
        onNewPbftBlock(*pbft_block);
      }

      break;
    }
    // need cert votes (syncing)
    case PbftBlockPacket: {
      if (syncing_state_.syncing_peer() != _nodeID) {
        LOG(log_wr_pbft_sync_) << "PbftBlockPacket received from unexpected peer " << _nodeID.abridged()
                               << " current syncing peer " << syncing_state_.syncing_peer().abridged();
        break;
      }

      auto pbft_blk_count = _r.itemCount();
      LOG(log_dg_pbft_sync_) << "PbftBlockPacket received, num of pbft blocks to be processed: " << pbft_blk_count;

      auto pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
      for (auto const pbft_blk_tuple : _r) {
        dev::RLP dag_blocks_rlp = pbft_blk_tuple[0];
        string received_dag_blocks_str;
        map<uint64_t, map<blk_hash_t, pair<DagBlock, vector<Transaction>>>> dag_blocks_per_level;
        for (auto const dag_blk_struct : dag_blocks_rlp) {
          DagBlock dag_blk(dag_blk_struct[0]);
          auto const &dag_blk_h = dag_blk.getHash();
          peer->markBlockAsKnown(dag_blk_h);
          std::vector<Transaction> new_transactions;
          for (auto const trx_raw : dag_blk_struct[1]) {
            auto &trx = new_transactions.emplace_back(trx_raw);
            peer->markTransactionAsKnown(trx.getHash());
          }
          received_dag_blocks_str += dag_blk_h.toString() + " ";
          auto level = dag_blk.getLevel();
          dag_blocks_per_level[level][dag_blk_h] = {move(dag_blk), move(new_transactions)};
        }
        LOG(log_dg_dag_sync_) << "Received Dag Blocks: " << received_dag_blocks_str;
        for (auto const &block_level : dag_blocks_per_level) {
          for (auto const &block : block_level.second) {
            auto status = checkDagBlockValidation(block.second.first);
            if (!status.first) {
              LOG(log_si_pbft_sync_) << "PBFT SYNC ERROR, DAG missing a tip/pivot"
                                     << ", PBFT chain size: " << pbft_chain_->getPbftChainSize()
                                     << ", synced queue size: " << pbft_chain_->pbftSyncedQueueSize();
              syncing_state_.set_peer_malicious();
              host->disconnect(_nodeID, p2p::UserReason);
              restartSyncingPbft(true);
              return;
            }
            LOG(log_nf_dag_sync_) << "Storing DAG block " << block.second.first.getHash().toString() << " with "
                                  << block.second.second.size() << " transactions";
            if (block.second.first.getLevel() > peer->dag_level_) {
              peer->dag_level_ = block.second.first.getLevel();
            }
            dag_blk_mgr_->processSyncedBlockWithTransactions(block.second.first, block.second.second);
          }
        }

        if (pbft_blk_tuple.itemCount() == 2) {
          PbftBlockCert pbft_blk_and_votes(pbft_blk_tuple[1]);
          auto pbft_blk_hash = pbft_blk_and_votes.pbft_blk->getBlockHash();
          peer->markPbftBlockAsKnown(pbft_blk_hash);
          LOG(log_dg_pbft_sync_) << "Processing pbft block: " << pbft_blk_and_votes.pbft_blk->getBlockHash();

          if (pbft_chain_->isKnownPbftBlockForSyncing(pbft_blk_hash)) {
            LOG(log_dg_pbft_sync_) << "Block " << pbft_blk_and_votes.pbft_blk->getBlockHash()
                                   << " already processed or scheduled to be processed";
            continue;
          }

          if (!pbft_chain_->checkPbftBlockValidationFromSyncing(*pbft_blk_and_votes.pbft_blk)) {
            LOG(log_er_pbft_sync_) << "Invalid PBFT block " << pbft_blk_hash << " from peer " << _nodeID.abridged()
                                   << " received, stop syncing.";
            syncing_state_.set_peer_malicious();
            host->disconnect(_nodeID, p2p::UserReason);
            restartSyncingPbft(true);
            return;
          }

          // Update peer's pbft period if outdated
          if (peer->pbft_chain_size_ < pbft_blk_and_votes.pbft_blk->getPeriod()) {
            peer->pbft_chain_size_ = pbft_blk_and_votes.pbft_blk->getPeriod();
          }

          // Notice: cannot verify 2t+1 cert votes here. Since don't
          // have correct account status for nodes which after the
          // first synced one.
          pbft_chain_->setSyncedPbftBlockIntoQueue(pbft_blk_and_votes);
          pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
          LOG(log_nf_pbft_sync_) << "Synced PBFT block hash " << pbft_blk_hash << " with "
                                 << pbft_blk_and_votes.cert_votes.size() << " cert votes";
          LOG(log_dg_pbft_sync_) << "Synced PBFT block " << pbft_blk_and_votes;

          // Reset last sync packet received time
          syncing_state_.set_last_sync_packet_time();
        }
      }
      if (pbft_blk_count > 0) {
        if (syncing_state_.is_pbft_syncing()) {
          if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * conf_.network_sync_level_size)) {
            LOG(log_dg_pbft_sync_) << "Syncing pbft blocks too fast than processing. Has synced period "
                                   << pbft_sync_period << ", PBFT chain size " << pbft_chain_->getPbftChainSize();
            tp_.post(1000, [this] { delayedPbftSync(1); });
          } else {
            syncPeerPbft(pbft_sync_period + 1);
          }
        }
      } else {
        LOG(log_dg_pbft_sync_) << "Syncing PBFT is completed";
        // We are pbft synced with the node we are connected to but
        // calling restartSyncingPbft will check if some nodes have
        // greater pbft chain size and we should continue syncing with
        // them, Or sync pending DAG blocks
        restartSyncingPbft(true);
        // We are pbft synced, send message to other node to start
        // gossiping new blocks
        if (!syncing_state_.is_pbft_syncing()) {
          // TODO: Why need to clear all DAG blocks and transactions?
          // This is inside PbftBlockPacket. Why don't clear PBFT blocks and votes?
          sendSyncedMessage();
        }
      }
      break;
    }
    case TestPacket: {
      LOG(log_dg_) << "Received TestPacket";
      ++cnt_received_messages_[_nodeID];
      test_sums_[_nodeID] += _r[0].toInt();
      BOOST_ASSERT(_id == TestPacket);
      break;
    }
  };
}

void TaraxaCapability::handle_read_exception(weak_ptr<Session> session, unsigned _packetType) {
  try {
    throw;
  } catch (std::exception const &_e) {
    // TODO be more precise about the error handling
    LOG(log_er_) << "Read exception: " << _e.what() << ". PacketType: " << packetTypeToString(_packetType) << " ("
                 << _packetType << ")";
    if (auto session_p = session.lock()) {
      session_p->disconnect(BadProtocol);
    }
  }
}

void TaraxaCapability::delayedPbftSync(int counter) {
  auto pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
  if (counter > 60) {
    LOG(log_er_pbft_sync_) << "Pbft blocks stuck in queue, no new block processed in 60 seconds " << pbft_sync_period
                           << " " << pbft_chain_->getPbftChainSize();
    syncing_state_.set_pbft_syncing(false);
    LOG(log_dg_pbft_sync_) << "Syncing PBFT is stopping";
    return;
  }

  if (syncing_state_.is_pbft_syncing()) {
    if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * conf_.network_sync_level_size)) {
      LOG(log_dg_pbft_sync_) << "Syncing pbft blocks faster than processing " << pbft_sync_period << " "
                             << pbft_chain_->getPbftChainSize();
      tp_.post(1000, [this, counter] { delayedPbftSync(counter + 1); });
    } else {
      syncPeerPbft(pbft_sync_period + 1);
    }
  }
}

void TaraxaCapability::restartSyncingPbft(bool force) {
  if (syncing_state_.is_pbft_syncing() && !force) {
    LOG(log_dg_pbft_sync_) << "restartSyncingPbft called but syncing_ already true";
    return;
  }

  NodeID max_pbft_chain_nodeID;
  uint64_t max_pbft_chain_size = 0;
  uint64_t max_node_dag_level = 0;

  if (!peers_state_.getPeersCount()) {
    LOG(log_nf_pbft_sync_) << "Restarting syncing PBFT not possible since no connected peers";
    return;
  }
  for (auto const &peer : peers_state_.getAllPeers()) {
    if (peer.second->pbft_chain_size_ > max_pbft_chain_size) {
      max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_nodeID = peer.first;
      max_node_dag_level = peer.second->dag_level_;
    } else if (peer.second->pbft_chain_size_ == max_pbft_chain_size && peer.second->dag_level_ > max_node_dag_level) {
      max_pbft_chain_nodeID = peer.first;
      max_node_dag_level = peer.second->dag_level_;
    }
  }

  auto pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
  if (max_pbft_chain_size > pbft_sync_period) {
    LOG(log_si_pbft_sync_) << "Restarting syncing PBFT from peer " << max_pbft_chain_nodeID << ", peer PBFT chain size "
                           << max_pbft_chain_size << ", own PBFT chain synced at period " << pbft_sync_period;
    syncing_state_.set_dag_syncing(false);
    syncing_state_.set_pbft_syncing(true, max_pbft_chain_nodeID);
    syncPeerPbft(pbft_sync_period + 1);
  } else {
    LOG(log_nf_pbft_sync_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period
                           << "(" << pbft_chain_->getPbftChainSize() << ")"
                           << " is greater or equal than max node pbft chain size:" << max_pbft_chain_size;
    syncing_state_.set_pbft_syncing(false);

    if (force || (!syncing_state_.is_dag_syncing() &&
                  max_node_dag_level > std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()))) {
      LOG(log_nf_dag_sync_) << "Request pending " << max_node_dag_level << " "
                            << std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()) << "("
                            << dag_mgr_->getMaxLevel() << ")";

      syncing_state_.set_dag_syncing(true, max_pbft_chain_nodeID);
      requestPendingDagBlocks();
    } else {
      syncing_state_.set_dag_syncing(false);
    }
  }
}

void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  LOG(log_nf_) << "Node " << _nodeID << " disconnected";
  peers_state_.erasePeer(_nodeID);
  tp_.post([=] {
    cnt_received_messages_.erase(_nodeID);
    test_sums_.erase(_nodeID);

    if (syncing_state_.is_pbft_syncing() && syncing_state_.syncing_peer() == _nodeID) {
      if (getPeersCount() > 0) {
        LOG(log_dg_pbft_sync_) << "Restart PBFT/DAG syncing due to syncing peer disconnect.";
        restartSyncingPbft(true);
      } else {
        LOG(log_dg_pbft_sync_) << "Stop PBFT/DAG syncing due to syncing peer disconnect and no other peers available.";
        syncing_state_.set_pbft_syncing(false);
        syncing_state_.set_dag_syncing(false);
      }
    }
  });
}

void TaraxaCapability::sendTestMessage(NodeID const &_id, int _x, std::vector<char> const &data) {
  sealAndSend(_id, TestPacket, RLPStream(2) << _x << data);
}

bool TaraxaCapability::sendStatus(NodeID const &_id, bool _initial) {
  bool success = false;

  if (dag_mgr_) {
    if (_initial) {
      LOG(log_dg_) << "Sending initial status message to " << _id << ", protocol version " << version()
                   << ", network id " << conf_.network_id << ", genesis " << dag_mgr_->get_genesis()
                   << ", node version " << TARAXA_VERSION;
    }

    auto dag_max_level = dag_mgr_->getMaxLevel();
    auto pbft_chain_size = pbft_chain_->getPbftChainSize();
    auto pbft_round = pbft_mgr_->getPbftRound();
    auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();
    LOG(log_dg_dag_sync_) << "Sending status message to " << _id << " with dag level: " << dag_max_level;
    LOG(log_dg_pbft_sync_) << "Sending status message to " << _id << " with pbft chain size: " << pbft_chain_size
                           << ", syncing: " << std::boolalpha << syncing_state_.is_pbft_syncing();
    LOG(log_dg_next_votes_sync_) << "Sending status message to " << _id << " with PBFT round: " << pbft_round
                                 << ", previous round next votes size " << pbft_previous_round_next_votes_size;

    if (_initial) {
      success = sealAndSend(_id, StatusPacket,
                            RLPStream(INITIAL_STATUS_PACKET_ITEM_COUNT)
                                << conf_.network_id << dag_max_level << dag_mgr_->get_genesis() << pbft_chain_size
                                << syncing_state_.is_pbft_syncing() << pbft_round << pbft_previous_round_next_votes_size
                                << TARAXA_MAJOR_VERSION << TARAXA_MINOR_VERSION << TARAXA_PATCH_VERSION);
    } else {
      success = sealAndSend(_id, StatusPacket,
                            RLPStream(5) << dag_max_level << pbft_chain_size << syncing_state_.is_pbft_syncing()
                                         << pbft_round << pbft_previous_round_next_votes_size);
    }
  }

  return success;
}

void TaraxaCapability::onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork) {
  if (fromNetwork) {
    if (dag_blk_mgr_) {
      LOG(log_nf_trx_prp_) << "Storing " << transactions.size() << " transactions";
      received_trx_count += transactions.size();
      unique_received_trx_count += trx_mgr_->insertBroadcastedTransactions(transactions);
    } else {
      for (auto const &transaction : transactions) {
        Transaction trx(transaction);
        auto trx_hash = trx.getHash();
        if (test_transactions_.find(trx_hash) == test_transactions_.end()) {
          test_transactions_[trx_hash] = trx;
          LOG(log_dg_trx_prp_) << "Received New Transaction " << trx_hash;
        } else {
          LOG(log_dg_trx_prp_) << "Received New Transaction" << trx_hash << "that is already known";
        }
      }
    }
  }
  if (!fromNetwork || conf_.network_transaction_interval == 0) {
    std::unordered_map<NodeID, std::vector<taraxa::bytes>> transactionsToSend;
    std::unordered_map<NodeID, std::vector<trx_hash_t>> transactionsHashToSend;
    auto peers = peers_state_.getAllPeers();

    for (auto &peer : peers) {
      // Confirm that status messages were exchanged otherwise message might be ignored and node would
      // incorrectly markTransactionAsKnown
      if (!peer.second->syncing_) {
        for (auto const &transaction : transactions) {
          Transaction trx(transaction);
          auto trx_hash = trx.getHash();
          if (!peer.second->isTransactionKnown(trx_hash)) {
            transactionsToSend[peer.first].push_back(transaction);
            transactionsHashToSend[peer.first].push_back(trx_hash);
          }
        }
      }
    }

    for (auto &it : transactionsToSend) {
      sendTransactions(it.first, it.second);
    }
    for (auto &it : transactionsHashToSend) {
      for (auto &it2 : it.second) {
        peers[it.first]->markTransactionAsKnown(it2);
      }
    }
  }
}

void TaraxaCapability::onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions) {
  LOG(log_nf_dag_prp_) << "Receive DagBlock " << block.getHash() << " #Trx" << transactions.size() << std::endl;
  if (dag_blk_mgr_) {
    LOG(log_nf_dag_prp_) << "Storing block " << block.getHash().toString() << " with " << transactions.size()
                         << " transactions";
    dag_blk_mgr_->insertBroadcastedBlockWithTransactions(block, transactions);

  } else if (test_blocks_.find(block.getHash()) == test_blocks_.end()) {
    test_blocks_[block.getHash()] = block;
    for (auto tr : transactions) {
      test_transactions_[tr.getHash()] = tr;
    }
    onNewBlockVerified(block);

  } else {
    LOG(log_dg_dag_prp_) << "Received NewBlock " << block.getHash().toString() << "that is already known";
    return;
  }
}

void TaraxaCapability::sendSyncedMessage() {
  LOG(log_dg_dag_sync_) << "sendSyncedMessage ";
  for (auto &peer : getAllPeersIDs()) {
    sealAndSend(peer, SyncedPacket, RLPStream(0));
  }
}

void TaraxaCapability::onNewBlockVerified(DagBlock const &block) {
  // If node is syncing this is an old block that has been verified - no block goosip is needed
  if (syncing_state_.is_syncing()) {
    return;
  }
  LOG(log_dg_dag_prp_) << "Verified NewBlock " << block.getHash().toString();
  auto const peersWithoutBlock =
      peers_state_.selectPeers([&](TaraxaPeer const &_peer) { return !_peer.isBlockKnown(block.getHash()); });

  auto const peersToSendNumber =
      std::min<std::size_t>(std::max<std::size_t>(conf_.network_min_dag_block_broadcast, std::sqrt(getPeersCount())),
                            conf_.network_max_dag_block_broadcast);

  std::vector<NodeID> peersToSend;
  std::vector<NodeID> peersToAnnounce;
  std::tie(peersToSend, peersToAnnounce) = PeersState::randomPartitionPeers(peersWithoutBlock, peersToSendNumber);

  for (NodeID const &peerID : peersToSend) {
    RLPStream ts;
    auto peer = peers_state_.getPeer(peerID);
    if (peer && !peer->syncing_) {
      sendBlock(peerID, block);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToSend.empty()) LOG(log_dg_dag_prp_) << "Sent block to " << peersToSend.size() << " peers";

  for (NodeID const &peerID : peersToAnnounce) {
    RLPStream ts;
    auto peer = peers_state_.getPeer(peerID);
    if (peer && !peer->syncing_) {
      sendBlockHash(peerID, block);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToAnnounce.empty()) LOG(log_dg_dag_prp_) << "Anounced block to " << peersToAnnounce.size() << " peers";
}

void TaraxaCapability::sendBlocks(NodeID const &_id, std::vector<std::shared_ptr<DagBlock>> blocks) {
  if (blocks.empty()) return;

  auto peer = peers_state_.getPeer(_id);
  if (!peer) return;

  taraxa::bytes packet_bytes;
  size_t packet_items_count = 0;
  size_t blocks_counter = 0;

  for (auto &block : blocks) {
    size_t dag_block_items_count = 0;
    size_t previous_block_packet_size = packet_bytes.size();

    // Add dag block rlp to the sent bytes array
    taraxa::bytes block_bytes = block->rlp(true);
    packet_bytes.insert(packet_bytes.end(), std::begin(block_bytes), std::end(block_bytes));
    dag_block_items_count++;  // + 1 new dag blog

    for (auto trx : block->getTrxs()) {
      auto t = trx_mgr_->getTransaction(trx);
      if (!t) {
        LOG(log_er_dag_sync_) << "Transacation " << trx << " is not available. SendBlocks canceled";
        // TODO: This can happen on stopping the node because network
        // is not stopped since network does not support restart,
        // better solution needed
        return;
      }

      // Add dag block transaction rlp to the sent bytes array
      packet_bytes.insert(packet_bytes.end(), std::begin(t->second), std::end(t->second));
      dag_block_items_count++;  // + 1 new tx from dag blog
    }

    LOG(log_tr_dag_sync_) << "Send DagBlock " << block->getHash() << "Trxs count: " << block->getTrxs().size();

    // Split packet into multiple smaller ones if total size is > MAX_PACKET_SIZE
    if (packet_bytes.size() > MAX_PACKET_SIZE) {
      LOG(log_dg_dag_sync_) << "Sending partial BlocksPacket due to MAX_PACKET_SIZE limit. " << blocks_counter
                            << " blocks out of " << blocks.size() << " PbftBlockPacketsent.";

      taraxa::bytes removed_bytes;
      std::copy(packet_bytes.begin() + previous_block_packet_size, packet_bytes.end(),
                std::back_inserter(removed_bytes));
      packet_bytes.resize(previous_block_packet_size);

      RLPStream s(packet_items_count + 1 /* final packet flag */);
      // As DagBlocksPacket might be split into multiple packets, we
      // need to differentiate if is the last one or not due to syncing
      s.append(false);  // flag if it is the final DagBlocksSyncPacket or not
      s.appendRaw(packet_bytes, packet_items_count);
      sealAndSend(_id, BlocksPacket, std::move(s));

      packet_bytes = std::move(removed_bytes);
      packet_items_count = 0;
    }

    packet_items_count += dag_block_items_count;
    blocks_counter++;
  }

  LOG(log_dg_dag_sync_) << "Sending final BlocksPacket with " << blocks_counter << " blocks.";

  RLPStream s(packet_items_count + 1 /* final packet flag */);
  s.append(true);  // flag if it is the final DagBlocksPacket or not
  s.appendRaw(packet_bytes, packet_items_count);
  sealAndSend(_id, BlocksPacket, std::move(s));
}

void TaraxaCapability::sendTransactions(NodeID const &_id, std::vector<taraxa::bytes> const &transactions) {
  LOG(log_nf_trx_prp_) << "sendTransactions " << transactions.size() << " to " << _id;
  RLPStream s(transactions.size());
  taraxa::bytes trx_bytes;
  for (auto transaction : transactions) {
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction), std::end(transaction));
  }
  s.appendRaw(trx_bytes, transactions.size());
  sealAndSend(_id, TransactionPacket, move(s));
}

void TaraxaCapability::sendBlock(NodeID const &_id, taraxa::DagBlock block) {
  vec_trx_t transactionsToSend;

  if (auto peer = peers_state_.getPeer(_id); peer) {
    for (auto trx : block.getTrxs())
      if (!peer->isTransactionKnown(trx)) transactionsToSend.push_back(trx);
  }

  RLPStream s(1 + transactionsToSend.size());
  s.appendRaw(block.rlp(true));

  taraxa::bytes trx_bytes;
  for (auto trx : transactionsToSend) {
    std::shared_ptr<std::pair<Transaction, taraxa::bytes>> transaction;
    if (dag_blk_mgr_) {
      transaction = trx_mgr_->getTransaction(trx);
    } else {
      assert(test_transactions_.find(trx) != test_transactions_.end());
      transaction = std::make_shared<std::pair<Transaction, taraxa::bytes>>(test_transactions_[trx],
                                                                            *test_transactions_[trx].rlp());
    }
    assert(transaction != nullptr);  // We should never try to send a block for
                                     // which  we do not have all transactions
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction->second), std::end(transaction->second));
  }
  s.appendRaw(trx_bytes, transactionsToSend.size());
  sealAndSend(_id, NewBlockPacket, move(s));
  LOG(log_dg_dag_prp_) << "Send DagBlock " << block.getHash() << " #Trx: " << transactionsToSend.size() << std::endl;
}

void TaraxaCapability::sendBlockHash(NodeID const &_id, taraxa::DagBlock block) {
  LOG(log_dg_dag_prp_) << "sendBlockHash " << block.getHash().toString();
  sealAndSend(_id, NewBlockHashPacket, RLPStream(1) << block.getHash());
}

void TaraxaCapability::requestBlock(NodeID const &_id, blk_hash_t hash) {
  LOG(log_dg_dag_prp_) << "requestBlock " << hash.toString();
  sealAndSend(_id, GetNewBlockPacket, RLPStream(1) << hash);
}

void TaraxaCapability::requestPbftBlocks(NodeID const &_id, size_t height_to_sync) {
  LOG(log_dg_pbft_sync_) << "Sending GetPbftBlockPacket with height: " << height_to_sync;
  sealAndSend(_id, GetPbftBlockPacket, RLPStream(1) << height_to_sync);
}

void TaraxaCapability::requestPendingDagBlocks() {
  vector<blk_hash_t> known_non_finalized_blocks;
  auto blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      known_non_finalized_blocks.push_back(block);
    }
  }
  requestBlocks(syncing_state_.syncing_peer(), known_non_finalized_blocks, KnownHashes);
}

std::pair<int, int> TaraxaCapability::retrieveTestData(NodeID const &_id) {
  int cnt = 0;
  int checksum = 0;
  for (auto i : cnt_received_messages_)
    if (_id == i.first) {
      cnt += i.second;
      checksum += test_sums_[_id];
    }

  return {cnt, checksum};
}

std::map<blk_hash_t, taraxa::DagBlock> TaraxaCapability::getBlocks() { return test_blocks_; }

std::map<trx_hash_t, taraxa::Transaction> TaraxaCapability::getTransactions() { return test_transactions_; }

void TaraxaCapability::sendTransactions() {
  if (trx_mgr_) {
    onNewTransactions(trx_mgr_->getNewVerifiedTrxSnapShotSerialized(), false);
    tp_.post(conf_.network_transaction_interval, [this] { sendTransactions(); });
  }
}

void TaraxaCapability::checkLiveness() {
  auto host = host_.lock();
  if (!host) {
    return;
  }
  for (auto const &peer : peers_state_.getAllPeers()) {
    // Disconnect any node that did not send any message for 3 status intervals
    if (!peer.second->isAlive(MAX_CHECK_ALIVE_COUNT)) {
      LOG(log_nf_) << "Host disconnected, no status message in " << MAX_CHECK_ALIVE_COUNT * check_alive_interval_
                   << " ms" << peer.first;
      host->disconnect(peer.first, p2p::PingTimeout);
    }
    // Send status message
    else {
      sendStatus(peer.first, false);
    }
  }
  tp_.post(check_alive_interval_, [this] { checkLiveness(); });
}

void TaraxaCapability::logNodeStats() {
  // TODO: Put this in its proper place and improve it...

  bool is_syncing = syncing_state_.is_syncing();

  NodeID max_pbft_round_nodeID;
  NodeID max_pbft_chain_nodeID;
  NodeID max_node_dag_level_nodeID;
  uint64_t peer_max_pbft_round = 1;
  uint64_t peer_max_pbft_chain_size = 1;
  uint64_t peer_max_node_dag_level = 1;
  const size_t peers_size = peers_state_.getPeersCount();
  for (auto const &peer : peers_state_.getAllPeers()) {
    // Find max pbft chain size
    if (peer.second->pbft_chain_size_ > peer_max_pbft_chain_size) {
      peer_max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_nodeID = peer.first;
    }

    // Find max dag level
    if (peer.second->dag_level_ > peer_max_node_dag_level) {
      peer_max_node_dag_level = peer.second->dag_level_;
      max_node_dag_level_nodeID = peer.first;
    }

    // Find max peer PBFT round
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      max_pbft_round_nodeID = peer.first;
    }
  }

  // Local dag info...
  auto local_max_level_in_dag = dag_mgr_->getMaxLevel();
  auto local_max_dag_level_in_queue = dag_blk_mgr_->getMaxDagLevelInQueue();

  // Local pbft info...
  uint64_t local_pbft_round = pbft_mgr_->getPbftRound();
  auto local_chain_size = pbft_chain_->getPbftChainSize();

  auto local_dpos_total_votes_count = pbft_mgr_->getDposTotalVotesCount();
  auto local_weighted_votes = pbft_mgr_->getDposWeightedVotesCount();
  auto local_twotplusone = pbft_mgr_->getTwoTPlusOne();

  // Syncing period...
  auto local_pbft_sync_period = pbft_chain_->pbftSyncingPeriod();

  // Decide if making progress...
  auto pbft_consensus_rounds_advanced = local_pbft_round - local_pbft_round_prev_interval_;
  auto pbft_chain_size_growth = local_chain_size - local_chain_size_prev_interval_;
  auto pbft_sync_period_progress = local_pbft_sync_period - local_pbft_sync_period_prev_interval_;
  auto dag_level_growh = local_max_level_in_dag - local_max_level_in_dag_prev_interval_;

  bool making_pbft_consensus_progress = (pbft_consensus_rounds_advanced > 0);
  bool making_pbft_chain_progress = (pbft_chain_size_growth > 0);
  bool making_pbft_sync_period_progress = (pbft_sync_period_progress > 0);
  bool making_dag_progress = (dag_level_growh > 0);

  LOG(log_dg_summary_) << "Making PBFT chain progress: " << std::boolalpha << making_pbft_chain_progress
                       << " (advanced " << pbft_chain_size_growth << " blocks)";
  if (is_syncing) {
    LOG(log_dg_summary_) << "Making PBFT sync period progress: " << std::boolalpha << making_pbft_sync_period_progress
                         << " (synced " << pbft_sync_period_progress << " blocks)";
  }
  LOG(log_dg_summary_) << "Making PBFT consensus progress: " << std::boolalpha << making_pbft_consensus_progress
                       << " (advanced " << pbft_consensus_rounds_advanced << " rounds)";
  LOG(log_dg_summary_) << "Making DAG progress: " << std::boolalpha << making_dag_progress << " (grew "
                       << dag_level_growh << " dag levels)";

  // Update syncing interval counts
  syncing_interval_count_ = syncing_state_.is_syncing() ? (syncing_interval_count_ + 1) : 0;
  syncing_stalled_interval_count_ = syncing_state_.is_syncing() && !making_pbft_chain_progress && !making_dag_progress
                                        ? (syncing_stalled_interval_count_ + 1)
                                        : 0;
  if (is_syncing) {
    intervals_syncing_since_launch++;
  } else {
    intervals_in_sync_since_launch++;
  }

  LOG(log_nf_summary_) << "Connected to " << peers_size << " peers";

  if (is_syncing) {
    // Syncing...
    auto percent_synced = (local_pbft_sync_period * 100) / peer_max_pbft_chain_size;
    auto syncing_time_sec = syncTimeSeconds();
    LOG(log_nf_summary_) << "Syncing for " << syncing_time_sec << " seconds, " << percent_synced << "% synced";
    LOG(log_nf_summary_) << "Currently syncing from node " << syncing_state_.syncing_peer();
    LOG(log_nf_summary_) << "Max peer PBFT chain size:      " << peer_max_pbft_chain_size << " (peer "
                         << max_pbft_chain_nodeID << ")";
    LOG(log_nf_summary_) << "Max peer PBFT consensus round:  " << peer_max_pbft_round << " (peer "
                         << max_pbft_round_nodeID << ")";
    LOG(log_nf_summary_) << "Max peer DAG level:             " << peer_max_node_dag_level << " (peer "
                         << max_node_dag_level_nodeID << ")";
  } else {
    auto sync_percentage =
        (100 * intervals_in_sync_since_launch) / (intervals_in_sync_since_launch + intervals_syncing_since_launch);
    LOG(log_nf_summary_) << "In sync since launch for " << sync_percentage << "% of the time";
    LOG(log_nf_summary_) << "Max DAG block level in DAG:    " << local_max_level_in_dag;
    LOG(log_nf_summary_) << "Max DAG block level in queue:  " << local_max_dag_level_in_queue;
    LOG(log_nf_summary_) << "PBFT chain size:               " << local_chain_size;
    LOG(log_nf_summary_) << "Current PBFT round:            " << local_pbft_round;
    LOG(log_nf_summary_) << "DPOS total votes count:        " << local_dpos_total_votes_count;
    LOG(log_nf_summary_) << "PBFT consensus 2t+1 threshold: " << local_twotplusone;
    LOG(log_nf_summary_) << "Node elligible vote count:     " << local_weighted_votes;

    LOG(log_dg_summary_) << "****** Memory structures sizes ******";
    LOG(log_dg_summary_) << "Unverified votes size:           " << vote_mgr_->getUnverifiedVotesSize();
    LOG(log_dg_summary_) << "Verified votes size:             " << vote_mgr_->getVerifiedVotesSize();

    auto [unverified_txs_size, verified_txs_size] = trx_mgr_->getTransactionQueueSize();
    LOG(log_dg_summary_) << "Unverified txs size:             " << unverified_txs_size;
    LOG(log_dg_summary_) << "Verified txs size:               " << verified_txs_size;
    LOG(log_dg_summary_) << "Txs buffer size:                 " << trx_mgr_->getTransactionBufferSize();

    auto [unverified_blocks_size, verified_blocks_size] = dag_blk_mgr_->getDagBlockQueueSize();
    auto [non_finalized_blocks_levels, non_finalized_blocks_size] = dag_mgr_->getNonFinalizedBlocksSize();
    auto [finalized_blocks_levels, finalized_blocks_size] = dag_mgr_->getFinalizedBlocksSize();
    LOG(log_dg_summary_) << "Unverified dag blocks size:      " << unverified_blocks_size;
    LOG(log_dg_summary_) << "Verified dag blocks size:        " << verified_blocks_size;
    LOG(log_dg_summary_) << "Non finalized dag blocks levels: " << non_finalized_blocks_levels;
    LOG(log_dg_summary_) << "Non finalized dag blocks size:   " << non_finalized_blocks_size;
    LOG(log_dg_summary_) << "Finalized dag blocks levels:     " << finalized_blocks_levels;
    LOG(log_dg_summary_) << "Finalized dag blocks size:       " << finalized_blocks_size;
  }

  LOG(log_nf_summary_) << "------------- tl;dr -------------";

  if (making_pbft_chain_progress) {
    if (is_syncing) {
      LOG(log_nf_summary_) << "STATUS: GOOD. ACTIVELY SYNCING";
    } else if (local_weighted_votes) {
      LOG(log_nf_summary_) << "STATUS: GOOD. NODE SYNCED AND PARTICIPATING IN CONSENSUS";
    } else {
      LOG(log_nf_summary_) << "STATUS: GOOD. NODE SYNCED";
    }
  } else if (is_syncing && (making_pbft_sync_period_progress || making_dag_progress)) {
    LOG(log_nf_summary_) << "STATUS: PENDING SYNCED DATA";
  } else if (!is_syncing && making_pbft_consensus_progress) {
    if (local_weighted_votes) {
      LOG(log_nf_summary_) << "STATUS: PARTICIPATING IN CONSENSUS BUT NO NEW FINALIZED BLOCKS";
    } else {
      LOG(log_nf_summary_) << "STATUS: NODE SYNCED BUT NO NEW FINALIZED BLOCKS";
    }
  } else if (!is_syncing && making_dag_progress) {
    LOG(log_nf_summary_) << "STATUS: PBFT STALLED, POSSIBLY PARTITIONED. NODE HAS NOT RESTARTED SYNCING";
  } else if (peers_size) {
    if (is_syncing) {
      auto syncing_stalled_time_sec = summary_interval_ms_ * syncing_stalled_interval_count_ / 1000;
      LOG(log_nf_summary_) << "STATUS: SYNCING STALLED. NO PROGRESS MADE IN LAST " << syncing_stalled_time_sec
                           << " SECONDS";
    } else {
      LOG(log_nf_summary_) << "STATUS: STUCK. NODE HAS NOT RESTARTED SYNCING";
    }
  } else {
    // Peer size is zero...
    LOG(log_nf_summary_) << "STATUS: NOT CONNECTED TO ANY PEERS. POSSIBLE CONFIG ISSUE OR NETWORK CONNECTIVITY";
  }

  LOG(log_nf_summary_) << "In the last " << std::setprecision(0) << summary_interval_ms_ / 1000 << " seconds...";

  if (is_syncing) {
    LOG(log_nf_summary_) << "PBFT sync period progress:      " << pbft_sync_period_progress;
  }
  {
    LOG(log_nf_summary_) << "PBFT chain blocks added:        " << pbft_chain_size_growth;
    LOG(log_nf_summary_) << "PBFT rounds advanced:           " << pbft_consensus_rounds_advanced;
    LOG(log_nf_summary_) << "DAG level growth:               " << dag_level_growh;
  }

  LOG(log_nf_summary_) << "##################################";

  // Node stats info history
  local_max_level_in_dag_prev_interval_ = local_max_level_in_dag;
  local_pbft_round_prev_interval_ = local_pbft_round;
  local_chain_size_prev_interval_ = local_chain_size;
  local_pbft_sync_period_prev_interval_ = local_pbft_sync_period;

  tp_.post(summary_interval_ms_, [this] { logNodeStats(); });
}

void TaraxaCapability::logPacketsStats() {
  static PacketsStats previous_received_packets_stats = received_packets_stats_;
  static PacketsStats previous_sent_packets_stats = sent_packets_stats_;

  LOG(log_nf_net_per_) << "Received packets stats: " << received_packets_stats_ - previous_received_packets_stats;
  LOG(log_nf_net_per_) << "Sent packets stats: " << sent_packets_stats_ - previous_sent_packets_stats;

  previous_received_packets_stats = received_packets_stats_;
  previous_sent_packets_stats = sent_packets_stats_;

  tp_.post(conf_.network_performance_log_interval, [this] { logPacketsStats(); });
}

void TaraxaCapability::onNewPbftVote(taraxa::Vote const &vote) {
  std::vector<NodeID> peers_to_send;

  for (auto const &peer : peers_state_.getAllPeers()) {
    if (!peer.second->isVoteKnown(vote.getHash())) {
      peers_to_send.push_back(peer.first);
    }
  }
  for (auto const &peerID : peers_to_send) {
    sendPbftVote(peerID, vote);
  }
}

void TaraxaCapability::sendPbftVote(NodeID const &peerID, taraxa::Vote const &vote) {
  auto peer = peers_state_.getPeer(peerID);
  // TODO: We should disable PBFT votes when a node is bootstrapping but not when trying to resync
  if (peer) {
    if (sealAndSend(peerID, PbftVotePacket, RLPStream(1) << vote.rlp(true))) {
      LOG(log_dg_vote_prp_) << "sendPbftVote " << vote.getHash() << " to " << peerID;
      peer->markVoteAsKnown(vote.getHash());
    }
  }
}

void TaraxaCapability::onNewPbftBlock(taraxa::PbftBlock const &pbft_block) {
  std::vector<NodeID> peers_to_send;
  auto my_chain_size = pbft_chain_->getPbftChainSize();

  for (auto const &peer : peers_state_.getAllPeers()) {
    if (!peer.second->isPbftBlockKnown(pbft_block.getBlockHash())) {
      peers_to_send.push_back(peer.first);
    }
  }

  for (auto const &peer : peers_to_send) {
    sendPbftBlock(peer, pbft_block, my_chain_size);
  }
}

// api for pbft syncing
void TaraxaCapability::sendPbftBlocks(NodeID const &peerID, size_t height_to_sync, size_t blocks_to_transfer) {
  LOG(log_dg_pbft_sync_) << "In sendPbftBlocks, peer want to sync from pbft chain height " << height_to_sync
                         << ", will send at most " << blocks_to_transfer << " pbft blocks to " << peerID;
  // If blocks_to_transfer is 0, will return empty PBFT blocks
  auto pbft_cert_blks = pbft_chain_->getPbftBlocks(height_to_sync, blocks_to_transfer);
  if (pbft_cert_blks.empty()) {
    sealAndSend(peerID, PbftBlockPacket, RLPStream(0));
    LOG(log_dg_pbft_sync_) << "In sendPbftBlocks, send no pbft blocks to " << peerID;
    return;
  }
  RLPStream s(pbft_cert_blks.size());
  // Example actual structure:
  // pbft_blk_1 -> [dag_blk_1, dag_blk_2]
  // pbft_blk_2 -> [dag_blk_3]
  // dag_blk_1 -> [trx_1, trx_2, trx_3]
  // dag_blk_2 -> [trx_4, trx_5, trx_6]
  // dag_blk_3 -> [trx_7, trx_8]
  //
  // Represented in the following variables:
  // pbft_blocks = [pbft_cert_blk_1, pbft_cert_blk_2]
  // pbft_blocks_extra = [pbft_blk_1_dag_blk_hashes, pbft_blk_2_dag_blk_hashes]
  // dag_blocks_indexes = [0, 2, 3]
  // dag_blocks = [dag_blk_1, dag_blk_2, dag_blk_3]
  // transactions_indexes = [0, 3, 6, 8]
  // transactions = [trx_1, trx_2, trx_3, trx_4, trx_5, trx_6, trx_7, trx_8]
  // General idea:
  // level_`k`[i] is parent of level_`k+1` elements with ordinals in range from (inclusive) edges_`k`_to_`k+1`[i] to
  // (exclusive) edges_`k`_to_`k+1`[i+1]

  DbStorage::MultiGetQuery db_query(db_);
  auto const &pbft_blocks = pbft_cert_blks;
  for (auto const &b : pbft_blocks) {
    db_query.append(DbStorage::Columns::dag_finalized_blocks, b.pbft_blk->getPivotDagBlockHash(), false);
  }
  auto pbft_blocks_extra = db_query.execute();

  // indexes to dag_blocks vector, based on which we know which dag blocks belong to which pbft block
  // pbft block 0 dag blocks indexes -> <dag_blocks_indexes[0], dag_blocks_indexes[1]> -> <start_idx, end_idx>
  // pbft block 0 dag blocks -> [dag_blocks[start_idx], dag_blocks[start_idx + 1], dag_blocks[end_idx]]
  //
  // pbft block 1 dag blocks indexes -> <dag_blocks_indexes[1], dag_blocks_indexes[2]> -> <start_idx, end_idx>
  // pbft block 1 dag blocks -> [dag_blocks[start_idx], dag_blocks[start_idx + 1], dag_blocks[end_idx]]
  //
  // pbft block 1 dag blocks indexes -> <dag_blocks_indexes[N], dag_blocks_indexes[N+1]> -> <start_idx, end_idx>
  // pbft block 1 dag blocks -> [dag_blocks[start_idx], dag_blocks[start_idx + 1], dag_blocks[end_idx]]
  vector<uint> dag_blocks_indexes;
  dag_blocks_indexes.reserve(1 + pbft_blocks.size());
  dag_blocks_indexes.push_back(0);
  for (uint i_0 = 0; i_0 < pbft_blocks.size(); ++i_0) {
    db_query.append(DbStorage::Columns::dag_blocks, RLP(pbft_blocks_extra[i_0]).toVector<h256>());
    dag_blocks_indexes.push_back(db_query.size());
  }
  auto dag_blocks = db_query.execute();

  // indexes to transactions vector, based on which we know which transactions belong to which dag block
  // dag block 0 transactions indexes -> <transactions_indexes[0], transactions_indexes[1]> -> <start_idx, end_idx>
  // dag block 0 transactions -> [transactions[start_idx], transactions[start_idx + 1], transactions[end_idx]]
  //
  // dag block 1 transactions indexes -> <transactions_indexes[1], transactions_indexes[2]> -> <start_idx, end_idx>
  // dag block 1 transactions -> [transactions[start_idx], transactions[start_idx + 1], transactions[end_idx]]
  //
  // dag block 1 transactions indexes -> <transactions_indexes[N], transactions_indexes[N+1]> -> <start_idx, end_idx>
  // dag block 1 transactions -> [transactions[start_idx], transactions[start_idx + 1], transactions[end_idx]]
  vector<uint> transactions_indexes;
  transactions_indexes.reserve(1 + dag_blocks.size());
  transactions_indexes.push_back(0);
  for (auto const &dag_blk_raw : dag_blocks) {
    db_query.append(DbStorage::Columns::transactions, DagBlock::extract_transactions_from_rlp(RLP(dag_blk_raw)));
    transactions_indexes.push_back(db_query.size());
  }
  auto transactions = db_query.execute();

  // Creates final packet out of provided pbft blocks rlp representations
  auto create_packet = [](std::vector<dev::bytes> &&pbft_blocks) -> RLPStream {
    RLPStream packet_rlp;
    packet_rlp.appendList(pbft_blocks.size());
    for (const dev::bytes &block_rlp : pbft_blocks) {
      packet_rlp.appendRaw(std::move(block_rlp));
    }

    return packet_rlp;
  };

  std::vector<dev::bytes> pbft_blocks_rlps;
  uint64_t act_packet_size = 0;

  // Iterate through pbft blocks
  for (uint pbft_block_idx = 0; pbft_block_idx < pbft_blocks.size(); ++pbft_block_idx) {
    RLPStream pbft_block_rlp;
    auto start_1 = dag_blocks_indexes[pbft_block_idx];
    auto end_1 = dag_blocks_indexes[pbft_block_idx + 1];

    std::vector<dev::bytes> dag_blocks_rlps;
    uint64_t dag_blocks_size = 0;

    // Iterate through dag blocks blocks / per pbft block
    for (uint dag_block_idx = start_1; dag_block_idx < end_1; ++dag_block_idx) {
      auto start_2 = transactions_indexes[dag_block_idx];
      auto end_2 = transactions_indexes[dag_block_idx + 1];

      RLPStream dag_rlp;
      dag_rlp.appendList(2);  // item #1 - dag block rlp, item #2 - list of dag block transactions
      dag_rlp.appendRaw(dag_blocks[dag_block_idx]);
      dag_rlp.appendList(end_2 - start_2);

      // Iterate through txs / per dag block
      for (uint trx_idx = start_2; trx_idx < end_2; ++trx_idx) {
        dag_rlp.appendRaw(transactions[trx_idx]);
      }

      // When checking if size limit exceeds MAX_PACKET_SIZE there are few bytes or rlp structure that is added
      // for the pbft block and dag block. This should be just a couple of bytes but we enforece even stricter 128
      // limit
      static const int RLP_OVERHEAD = 128;

      // Check if PBFT blocks need to be split and sent in multiple packets so we dont exceed
      // MAX_PACKET_SIZE (15 MB) limit
      if (act_packet_size + dag_blocks_size + dag_rlp.out().size() + pbft_blocks[pbft_block_idx].rlp().size() +
              RLP_OVERHEAD >
          MAX_PACKET_SIZE) {
        LOG(log_dg_dag_sync_) << "Sending partial PbftBlockPacket due tu MAX_PACKET_SIZE limit. " << pbft_block_idx + 1
                              << " blocks out of " << pbft_blocks.size() << " sent.";

        pbft_block_rlp.appendList(1);  // Only list of dag blocks and no pbft block rlp for incomplete packet
        pbft_block_rlp.appendList(dag_blocks_rlps.size());

        for (auto &dag_block : dag_blocks_rlps) pbft_block_rlp.appendRaw(dag_block);

        pbft_blocks_rlps.emplace_back(pbft_block_rlp.invalidate());

        // Send partial packet
        sealAndSend(peerID, PbftBlockPacket, create_packet(std::move(pbft_blocks_rlps)));

        act_packet_size = 0;
        pbft_blocks_rlps.clear();
        dag_blocks_size = 0;
        dag_blocks_rlps.clear();
      }

      dag_blocks_rlps.push_back(dag_rlp.out());
      dag_blocks_size += dag_rlp.out().size();
    }
    pbft_block_rlp.appendList(2);  // item #1 - list of dag blocks, item #2 pbft block rlp
    pbft_block_rlp.appendList(dag_blocks_rlps.size());

    for (auto &dag_block : dag_blocks_rlps) pbft_block_rlp.appendRaw(dag_block);

    pbft_block_rlp.appendRaw(pbft_blocks[pbft_block_idx].rlp());

    act_packet_size += pbft_block_rlp.out().size();
    pbft_blocks_rlps.emplace_back(pbft_block_rlp.invalidate());
  }

  // Send final packet
  sealAndSend(peerID, PbftBlockPacket, create_packet(std::move(pbft_blocks_rlps)));
  LOG(log_dg_pbft_sync_) << "Sending final PbftBlockPacket to " << peerID;
}

void TaraxaCapability::sendPbftBlock(NodeID const &peerID, taraxa::PbftBlock const &pbft_block,
                                     uint64_t const &pbft_chain_size) {
  LOG(log_dg_pbft_prp_) << "sendPbftBlock " << pbft_block.getBlockHash() << " to " << peerID;
  RLPStream s(2);
  pbft_block.streamRLP(s, true);
  s << pbft_chain_size;
  sealAndSend(peerID, NewPbftBlockPacket, move(s));
}

void TaraxaCapability::syncPbftNextVotes(uint64_t const pbft_round, size_t const pbft_previous_round_next_votes_size) {
  NodeID peer_node_ID;
  uint64_t peer_max_pbft_round = 1;
  size_t peer_max_previous_round_next_votes_size = 0;
  const auto peers = peers_state_.getAllPeers();

  // Find max peer PBFT round
  for (auto const &peer : peers) {
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      peer_node_ID = peer.first;
    }
  }

  if (pbft_round == peer_max_pbft_round) {
    // No peers ahead, find peer PBFT previous round max next votes size
    for (auto const &peer : peers) {
      if (peer.second->pbft_previous_round_next_votes_size_ > peer_max_previous_round_next_votes_size) {
        peer_max_previous_round_next_votes_size = peer.second->pbft_previous_round_next_votes_size_;
        peer_node_ID = peer.first;
      }
    }
  }

  if (pbft_round < peer_max_pbft_round ||
      (pbft_round == peer_max_pbft_round &&
       pbft_previous_round_next_votes_size < peer_max_previous_round_next_votes_size)) {
    LOG(log_dg_next_votes_sync_) << "Syncing PBFT next votes. Current PBFT round " << pbft_round
                                 << ", previous round next votes size " << pbft_previous_round_next_votes_size
                                 << ". Peer " << peer_node_ID << " is in PBFT round " << peer_max_pbft_round
                                 << ", previous round next votes size " << peer_max_previous_round_next_votes_size;
    requestPbftNextVotes(peer_node_ID, pbft_round, pbft_previous_round_next_votes_size);
  }
}

void TaraxaCapability::requestPbftNextVotes(NodeID const &peerID, uint64_t const pbft_round,
                                            size_t const pbft_previous_round_next_votes_size) {
  LOG(log_dg_next_votes_sync_) << "Sending GetPbftNextVotes with round " << pbft_round
                               << " previous round next votes size " << pbft_previous_round_next_votes_size;
  sealAndSend(peerID, GetPbftNextVotes, RLPStream(2) << pbft_round << pbft_previous_round_next_votes_size);
}

void TaraxaCapability::sendPbftNextVotes(NodeID const &peerID, std::vector<Vote> const &send_next_votes_bundle) {
  if (send_next_votes_bundle.empty()) {
    return;
  }

  RLPStream s(send_next_votes_bundle.size());
  for (auto const &next_vote : send_next_votes_bundle) {
    s.appendRaw(next_vote.rlp());
    LOG(log_dg_next_votes_sync_) << "Send out next vote " << next_vote.getHash() << " to peer " << peerID;
  }

  if (sealAndSend(peerID, PbftNextVotesPacket, move(s))) {
    LOG(log_nf_next_votes_sync_) << "Send out size of " << send_next_votes_bundle.size() << " PBFT next votes to "
                                 << peerID;
    if (auto peer = peers_state_.getPeer(peerID)) {
      for (auto const &v : send_next_votes_bundle) {
        peer->markVoteAsKnown(v.getHash());
      }
    }
  }
}

void TaraxaCapability::broadcastPreviousRoundNextVotesBundle() {
  auto next_votes_bundle = next_votes_mgr_->getNextVotes();
  if (next_votes_bundle.empty()) {
    LOG(log_er_next_votes_sync_) << "There are empty next votes for previous PBFT round";
    return;
  }

  auto pbft_current_round = pbft_mgr_->getPbftRound();

  for (auto const &peer : peers_state_.getAllPeers()) {
    // Nodes may vote at different values at previous round, so need less or equal
    if (!peer.second->syncing_ && peer.second->pbft_round_ <= pbft_current_round) {
      std::vector<Vote> send_next_votes_bundle;
      for (auto const &v : next_votes_bundle) {
        if (!peer.second->isVoteKnown(v.getHash())) {
          send_next_votes_bundle.emplace_back(v);
        }
      }
      sendPbftNextVotes(peer.first, send_next_votes_bundle);
    }
  }
}

Json::Value TaraxaCapability::getStatus() const {
  Json::Value res;
  NodeID max_pbft_round_nodeID;
  NodeID max_pbft_chain_nodeID;
  NodeID max_node_dag_level_nodeID;
  uint64_t peer_max_pbft_round = 1;
  uint64_t peer_max_pbft_chain_size = 1;
  uint64_t peer_max_node_dag_level = 1;

  res["peers"] = Json::Value(Json::arrayValue);
  for (auto &peer : peers_state_.getAllPeers()) {
    Json::Value peer_status;
    peer_status["node_id"] = peer.first.toString();
    peer_status["dag_level"] = Json::UInt64(peer.second->dag_level_);
    peer_status["pbft_size"] = Json::UInt64(peer.second->pbft_chain_size_);
    peer_status["dag_synced"] = !peer.second->syncing_;
    res["peers"].append(peer_status);
    // Find max pbft chain size
    if (peer.second->pbft_chain_size_ > peer_max_pbft_chain_size) {
      peer_max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_pbft_chain_nodeID = peer.first;
    }

    // Find max dag level
    if (peer.second->dag_level_ > peer_max_node_dag_level) {
      peer_max_node_dag_level = peer.second->dag_level_;
      max_node_dag_level_nodeID = peer.first;
    }

    // Find max peer PBFT round
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      max_pbft_round_nodeID = peer.first;
    }
  }

  if (syncing_state_.is_syncing()) {
    res["syncing_from_node_id"] = syncing_state_.syncing_peer().toString();
  }

  res["peer_max_pbft_round"] = Json::UInt64(peer_max_pbft_round);
  res["peer_max_pbft_chain_size"] = Json::UInt64(peer_max_pbft_chain_size);
  res["peer_max_node_dag_level"] = Json::UInt64(peer_max_node_dag_level);
  res["peer_max_pbft_round_node_id"] = max_pbft_round_nodeID.toString();
  res["peer_max_pbft_chain_size_node_id"] = max_pbft_chain_nodeID.toString();
  res["peer_max_node_dag_level_node_id"] = max_node_dag_level_nodeID.toString();

  auto createPacketsStatsJson = [&](const PacketsStats &stats) -> Json::Value {
    Json::Value stats_json;
    for (uint8_t it = 0; it != PacketCount; it++) {
      Json::Value packet_stats_json;
      const auto packet_stats = stats.getPacketStats(packetTypeToString(it));
      if (packet_stats == std::nullopt) {
        continue;
      }

      auto total = packet_stats->total_count_;
      packet_stats_json["total"] = Json::UInt64(total);
      if (total > 0) {
        packet_stats_json["avg packet size"] = Json::UInt64(packet_stats->total_size_ / total);
        packet_stats_json["avg packet processing duration"] =
            Json::UInt64(packet_stats->total_duration_.count() / total);
        auto unique = packet_stats->total_unique_count_;
        if (unique > 0) {
          packet_stats_json["unique"] = Json::UInt64(unique);
          packet_stats_json["unique %"] = Json::UInt64(unique * 100 / total);
          packet_stats_json["unique avg packet size"] = Json::UInt64(packet_stats->total_unique_size_ / unique);
          packet_stats_json["unique avg packet processing duration"] =
              Json::UInt64(packet_stats->total_unique_duration_.count() / unique);
        }
        stats_json[packetTypeToString(it)] = packet_stats_json;
      }
    }

    return stats_json;
  };

  Json::Value received_packet_stats_json = createPacketsStatsJson(received_packets_stats_);

  received_packet_stats_json["transaction count"] = Json::UInt64(received_trx_count);
  received_packet_stats_json["unique transaction count"] = Json::UInt64(unique_received_trx_count);
  if (received_trx_count)
    received_packet_stats_json["unique transaction %"] =
        Json::UInt64(unique_received_trx_count * 100 / received_trx_count);
  res["received packets stats"] = received_packet_stats_json;

  Json::Value sent_packet_stats_json = createPacketsStatsJson(sent_packets_stats_);
  res["sent packets stats"] = sent_packet_stats_json;

  return res;
}

string TaraxaCapability::packetTypeToString(unsigned int packet) const {
  switch (packet) {
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
    case PacketCount:
      return "PacketCount";
    case SyncedPacket:
      return "SyncedPacket";
    case SyncedResponsePacket:
      return "SyncedResponsePacket";
  }
  return "unknown packet type: " + std::to_string(packet);
}

}  // namespace taraxa
