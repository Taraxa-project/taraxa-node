#include "taraxa_capability.hpp"

#include "dag.hpp"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "pbft_manager.hpp"
#include "vote.hpp"
#include "transaction_manager.hpp"

using namespace taraxa;

std::shared_ptr<TaraxaPeer> TaraxaCapability::getPeer(NodeID const &node_id) {
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  auto itPeer = peers_.find(node_id);
  if (itPeer != peers_.end()) {
    return itPeer->second;
  }
  return nullptr;
}

unsigned int TaraxaCapability::getPeersCount() {
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  return peers_.size();
}

void TaraxaCapability::erasePeer(NodeID const &node_id) {
  boost::unique_lock<boost::shared_mutex> lock(peers_mutex_);
  peers_.erase(node_id);
}

void TaraxaCapability::insertPeer(NodeID const &node_id,
                                  std::shared_ptr<TaraxaPeer> const &peer) {
  boost::unique_lock<boost::shared_mutex> lock(peers_mutex_);
  peers_.emplace(
      std::make_pair(node_id, std::make_shared<TaraxaPeer>(node_id)));
}

void TaraxaCapability::syncPeerPbft(NodeID const &_nodeID,
                                    unsigned long height_to_sync) {
  if (full_node_.lock()) {
    LOG(log_nf_pbft_sync_) << "Sync peer node " << _nodeID
                           << " from pbft chain height " << height_to_sync;
    requestPbftBlocks(_nodeID, height_to_sync);
  }
}

std::pair<bool, blk_hash_t> TaraxaCapability::checkTipsandPivot(
    DagBlock const &block) {
  level_t expected_level = 0;
  if (auto full_node = full_node_.lock()) {
    for (auto const &tip : block.getTips()) {
      auto tip_block = full_node->getBlockManager()->getDagBlock(tip);
      if (!tip_block) {
        LOG(log_nf_dag_sync_) << "Block " << block.getHash().toString()
                              << " has a missing tip " << tip.toString();
        return std::make_pair(false, tip);
      }
      expected_level = std::max(tip_block->getLevel(), expected_level);
    }
    auto pivot = block.getPivot();
    auto pivot_block = full_node->getBlockManager()->getDagBlock(pivot);
    if (!pivot_block) {
      LOG(log_nf_) << "Block " << block.getHash().toString()
                   << " has a missing pivot " << pivot.toString();
      return std::make_pair(false, pivot);
    }
    expected_level = std::max(pivot_block->getLevel(), expected_level);
    expected_level++;
    if (expected_level != block.getLevel()) {

      throw InvalidDataException(std::string("Invalid block level ") +
                                 std::to_string(block.getLevel()) +
                                 " for block " + block.getHash().toString() +
                                 ". Expected level " +
                                 std::to_string(expected_level));
    }
  }
  return std::make_pair(true, blk_hash_t()); 
}

void TaraxaCapability::onConnect(NodeID const &_nodeID, u256 const &) {
  LOG(log_nf_) << "Node " << _nodeID << " connected";
  cnt_received_messages_[_nodeID] = 0;
  test_sums_[_nodeID] = 0;

  insertPeer(_nodeID, std::make_shared<TaraxaPeer>(_nodeID));
  sendStatus(_nodeID, true);
}

bool TaraxaCapability::interpretCapabilityPacket(NodeID const &_nodeID,
                                                 unsigned _id, RLP const &_r) {
  if (stopped_) return true;
  if (conf_.network_simulated_delay == 0) {
    if (performance_log_) {
      static std::map<unsigned, std::pair<uint32_t, uint64_t>> perf_data;
      static std::chrono::steady_clock::time_point begin_perf =
          std::chrono::steady_clock::now();
      std::chrono::steady_clock::time_point begin =
          std::chrono::steady_clock::now();
      auto ret = interpretCapabilityPacketImpl(_nodeID, _id, _r);
      std::chrono::steady_clock::time_point end =
          std::chrono::steady_clock::now();
      perf_data[_id].first++;
      perf_data[_id].second +=
          std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
              .count();
      if (std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - begin_perf)
              .count() > 20) {
        uint32_t total_count = 0;
        uint64_t total_time = 0;
        for (auto const &it : perf_data) {
          total_count += it.second.first;
          total_time += it.second.second;
          LOG(log_perf_) << packetToPacketName(it.first)
                         << " No: " << it.second.first << " - Avg time: "
                         << it.second.second / it.second.first << "[µs]"
                         << std::endl;
        }
        LOG(log_perf_) << "All packets"
                       << " No: " << total_count
                       << " - Avg time: " << total_time / total_count << "[µs]"
                       << std::endl;
        begin_perf = end;
      }

      return ret;
    } else
      return interpretCapabilityPacketImpl(_nodeID, _id, _r);
  }
  // RLP contains memory it does not own so deep copy of bytes is needed
  dev::bytes rBytes = _r.data().toBytes();
  int messageSize = rBytes.size() * 8;
  unsigned int dist =
      *((int *)this->host_.id().data()) ^ *((int *)_nodeID.data());
  unsigned int delay = dist % conf_.network_simulated_delay;

  auto bandwidth = conf_.network_bandwidth ? conf_.network_bandwidth : 40;
  unsigned int bandwidth_delay = messageSize / (bandwidth * 1000);  // in ms

  // Random component up to +-10%
  int random_component = random_dist_(delay_rng_);

  unsigned int total_delay = (delay + bandwidth_delay) * random_component / 100;
  LOG(log_dg_) << "Delaying packet by: (" << delay << " , " << bandwidth_delay
               << " ), actual delay =" << total_delay << " milliseconds";
  auto timer = std::make_shared<boost::asio::deadline_timer>(io_service_);
  timer->expires_from_now(boost::posix_time::milliseconds(total_delay));
  timer->async_wait((
      [this, _nodeID, _id, rBytes, timer](const boost::system::error_code &ec) {
        RLP _rCopy(rBytes);
        interpretCapabilityPacketImpl(_nodeID, _id, _rCopy);
      }));
  return true;
}

bool TaraxaCapability::processSyncDagBlocks(NodeID const &_nodeID) {
  auto full_node = full_node_.lock();
  if (full_node && requesting_pending_dag_blocks_) {
    auto peer = getPeer(_nodeID);
    for (auto block_level = peer->sync_blocks_.begin();
         block_level != peer->sync_blocks_.end(); block_level++) {
      for (auto block : block_level->second) {
        auto status = checkTipsandPivot(block.second.first);
        if (!status.first) {
          LOG(log_nf_dag_sync_)
              << "Missing a tip or pivot, requesting it" << status.second;
          requestBlock(_nodeID, status.second, false);
          return false;
        }

        LOG(log_nf_dag_sync_)
            << "Storing block " << block.second.first.getHash().toString()
            << " with " << block.second.second.size() << " transactions";
        if (block.second.first.getLevel() > peer->dag_level_)
          peer->dag_level_ = block.second.first.getLevel();
        full_node->getBlockManager()->insertBroadcastedBlockWithTransactions(block.second.first,
                                                          block.second.second);
      }
    }
  }
  return true;
}

bool TaraxaCapability::interpretCapabilityPacketImpl(NodeID const &_nodeID,
                                                     unsigned _id,
                                                     RLP const &_r) {
  try {
    auto peer = getPeer(_nodeID);
    if (peer) {
      switch (_id) {
        case SyncedPacket: {
          LOG(log_dg_dag_sync_) << "Received synced message from " << _nodeID;
          peer->syncing_ = false;
          peer->clearAllKnownBlocksAndTransactions();
        } break;
        case StatusPacket: {
          peer->statusReceived();
          if (auto full_node = full_node_.lock()) {
            bool initial_status = _r.itemCount() == 6;
            uint64_t peer_level;
            uint64_t peer_pbft_chain_size;
            auto pbft_chain_size = full_node->getPbftChainSize();
            if (initial_status) {
              auto const peer_protocol_version = _r[0].toInt<unsigned>();
              auto const network_id = _r[1].toString();
              peer_level = _r[2].toPositiveInt64();
              auto const genesis_hash = _r[3].toString();
              peer_pbft_chain_size = _r[4].toPositiveInt64();
              peer->syncing_ = _r[5].toInt();
              LOG(log_dg_) << "Received initial status message from " << _nodeID
                           << " " << peer_protocol_version << " " << network_id
                           << " " << peer_level << " " << genesis_ << " "
                           << peer_pbft_chain_size << " " << peer->syncing_;

              if (peer_protocol_version != c_protocolVersion) {
                LOG(log_er_)
                    << "Incorrect protocol version " << peer_protocol_version
                    << ", host " << _nodeID << " will be disconnected";
                host_.capabilityHost()->disconnect(_nodeID, p2p::UserReason);
              }
              if (network_id != conf_.network_id) {
                LOG(log_er_) << "Incorrect network id " << network_id
                             << ", host " << _nodeID << " will be disconnected";
                host_.capabilityHost()->disconnect(_nodeID, p2p::UserReason);
              }
              if (genesis_hash != genesis_) {
                LOG(log_er_) << "Incorrect genesis hash " << genesis_hash
                             << ", host " << _nodeID << " will be disconnected";
                host_.capabilityHost()->disconnect(_nodeID, p2p::UserReason);
              }
              // Only on the initial status message the other node might not
              // have still started syncing so double check with pbft chain size
              peer->syncing_ |= peer_pbft_chain_size < pbft_chain_size;
            } else {
              peer_level = _r[0].toPositiveInt64();
              peer_pbft_chain_size = _r[1].toPositiveInt64();
              peer->syncing_ = _r[2].toInt();
              LOG(log_dg_) << "Received status message from " << _nodeID << " "
                           << peer_level << " " << peer_pbft_chain_size;
            }
            LOG(log_dg_dag_sync_) << "Received status message from " << _nodeID
                                  << " DAG level:" << peer_level;
            LOG(log_dg_pbft_sync_)
                << "Received status message from " << _nodeID
                << " PBFT chain size:" << peer_pbft_chain_size << " "
                << peer->syncing_;
            peer->pbft_chain_size_ = peer_pbft_chain_size;
            peer->dag_level_ = peer_level;
            LOG(log_dg_pbft_sync_)
                << "peer_pbft_chain_size: " << peer_pbft_chain_size
                << "peer_syncing_pbft_chain_size_: "
                << peer_syncing_pbft_chain_size_;
            if (peer->syncing_) {
              LOG(log_dg_pbft_sync_)
                  << "Other node is behind, prevent gossiping " << _nodeID
                  << "Our pbft level: " << pbft_chain_size
                  << " Peer level: " << peer_pbft_chain_size;
              if (syncing_ && peer_syncing_pbft == _nodeID) {
                // We are currently syncing to a node that just reported it is
                // not synced, force a switch to a new node
                restartSyncingPbft(true);
              }
            }
          }
          break;
        }
        // Means a new block is proposed, full block body and all transaction
        // are received.
        case NewBlockPacket: {
          DagBlock block(_r[0].data().toBytes());

          auto transactionsCount = _r.itemCount() - 1;
          LOG(log_dg_dag_prp_)
              << "Received NewBlockPacket " << transactionsCount;

          std::vector<Transaction> newTransactions;
          for (auto iTransaction = 1; iTransaction < transactionsCount + 1;
               iTransaction++) {
            Transaction transaction(_r[iTransaction].data().toBytes());
            newTransactions.push_back(transaction);
            peer->markTransactionAsKnown(transaction.getHash());
          }

          peer->markBlockAsKnown(block.getHash());
          if (block.getLevel() > peer->dag_level_)
            peer->dag_level_ = block.getLevel();
          onNewBlockReceived(block, newTransactions);
          break;
        }
        // Full block and partial transactions are received
        case BlockPacket: {
          DagBlock block(_r[0].data().toBytes());

          auto transactionsCount = _r.itemCount() - 1;
          std::vector<Transaction> newTransactions;
          for (auto iTransaction = 1; iTransaction < transactionsCount + 1;
               iTransaction++) {
            Transaction transaction(_r[iTransaction].data().toBytes());
            newTransactions.emplace_back(transaction);
            peer->markTransactionAsKnown(transaction.getHash());
          }

          LOG(log_dg_dag_sync_)
              << "Received BlockPacket " << block.getHash().toString();
          peer->markBlockAsKnown(block.getHash());
          peer->sync_blocks_[block.getLevel()][block.getHash()] = {
              block, newTransactions};
          if (processSyncDagBlocks(_nodeID)) {
            auto level_to_sync = peer->sync_blocks_.rbegin()->first + 1;
            peer->sync_blocks_.clear();
            requestPendingDagBlocks(_nodeID, level_to_sync);
          }
          break;
        }

        case NewBlockHashPacket: {
          blk_hash_t hash(_r[0]);
          LOG(log_dg_dag_prp_)
              << "Received NewBlockHashPacket" << hash.toString();
          peer->markBlockAsKnown(hash);
          if (auto full_node = full_node_.lock()) {
            if (!full_node->getBlockManager()->isBlockKnown(hash) &&
                block_requestes_set_.count(hash) == 0) {
              block_requestes_set_.insert(hash);
              requestBlock(_nodeID, hash, true);
            }
          } else if (test_blocks_.find(hash) == test_blocks_.end() &&
                     block_requestes_set_.count(hash) == 0) {
            block_requestes_set_.insert(hash);
            requestBlock(_nodeID, hash, true);
          }
          break;
        }
        case GetBlockPacket: {
          blk_hash_t hash(_r[0]);
          LOG(log_dg_dag_sync_) << "Received GetBlockPacket" << hash.toString();
          peer->markBlockAsKnown(hash);
          if (auto full_node = full_node_.lock()) {
            auto block = full_node->getDB()->getDagBlock(hash);
            if (block) {
              sendBlock(_nodeID, *block, false);
            } else
              LOG(log_nf_dag_sync_) << "NO PACKET: " << hash.toString();
          }
          break;
        }

        case GetNewBlockPacket: {
          blk_hash_t hash(_r[0]);
          peer->markBlockAsKnown(hash);
          LOG(log_dg_dag_prp_)
              << "Received GetNewBlockPacket" << hash.toString();

          if (auto full_node = full_node_.lock()) {
            auto block = full_node->getDB()->getDagBlock(hash);
            if (block) {
              sendBlock(_nodeID, *block, true);
            } else
              LOG(log_nf_dag_prp_) << "NO NEW PACKET: " << hash.toString();
          } else if (test_blocks_.find(hash) != test_blocks_.end()) {
            sendBlock(_nodeID, test_blocks_[hash], true);
          }
          break;
        }
        case GetBlocksPacket: {
          level_t level = _r[0].toInt();
          LOG(log_dg_dag_sync_) << "Received GetBlocksPacket " << level;
          if (auto full_node = full_node_.lock()) {
            auto dag_blocks = full_node->getDB()->getDagBlocksAtLevel(
                level, conf_.network_sync_level_size);
            sendBlocks(_nodeID, dag_blocks);
          }
          break;
        }
        case GetLeavesBlocksPacket: {
          LOG(log_dg_dag_sync_) << "Received GetLeavesBlocksPacket";
          if (auto full_node = full_node_.lock()) {
            std::vector<std::string> dag_blocks;
            full_node->getDagManager()->collectTotalLeaves(dag_blocks);
            sendLeavesBlocks(_nodeID, dag_blocks);
          }
          break;
        }
        case BlocksPacket: {
          std::string received_dag_blocks_str;
          auto itemCount = _r.itemCount();
          int transactionCount = 0;
          for (auto iBlock = 0; iBlock < itemCount; iBlock++) {
            DagBlock block(_r[iBlock + transactionCount].data().toBytes());
            peer->markBlockAsKnown(block.getHash());

            std::vector<Transaction> newTransactions;
            for (int i = 0; i < block.getTrxs().size(); i++) {
              transactionCount++;
              Transaction transaction(
                  _r[iBlock + transactionCount].data().toBytes());
              newTransactions.push_back(transaction);
              peer->markTransactionAsKnown(transaction.getHash());
            }

            received_dag_blocks_str += block.getHash().toString() + " ";
            peer->sync_blocks_[block.getLevel()][block.getHash()] = {
                block, newTransactions};
            if (iBlock + transactionCount + 1 >= itemCount) break;
          }

          auto full_node = full_node_.lock();
          LOG(log_nf_dag_sync_)
              << "Received Dag Blocks: " << received_dag_blocks_str;
          if (itemCount > 0) {
            if (processSyncDagBlocks(_nodeID)) {
              auto level_to_sync = peer->sync_blocks_.rbegin()->first + 1;
              peer->sync_blocks_.clear();
              requestPendingDagBlocks(_nodeID, level_to_sync);
            }
          } else {
            requestLeavesDagBlocks(_nodeID);
          }
          break;
        }
        case LeavesBlocksPacket: {
          std::string received_dag_blocks_str;
          auto itemCount = _r.itemCount();
          int transactionCount = 0;
          for (auto iBlock = 0; iBlock < itemCount; iBlock++) {
            DagBlock block(_r[iBlock + transactionCount].data().toBytes());
            peer->markBlockAsKnown(block.getHash());

            std::vector<Transaction> newTransactions;
            for (int i = 0; i < block.getTrxs().size(); i++) {
              transactionCount++;
              Transaction transaction(
                  _r[iBlock + transactionCount].data().toBytes());
              newTransactions.push_back(transaction);
              peer->markTransactionAsKnown(transaction.getHash());
            }

            received_dag_blocks_str += block.getHash().toString() + " ";
            peer->sync_blocks_[block.getLevel()][block.getHash()] = {
                block, newTransactions};
            if (iBlock + transactionCount + 1 >= itemCount) break;
          }

          auto full_node = full_node_.lock();
          LOG(log_nf_dag_sync_)
              << "Received Dag Blocks: " << received_dag_blocks_str;
          if (itemCount == 0 || processSyncDagBlocks(_nodeID)) {
            peer->sync_blocks_.clear();
            requesting_pending_dag_blocks_ = false;
            if (!syncing_) {
              LOG(log_dg_dag_sync_) << "Syncing is stopping";
              sendSyncedMessage();
            }
          }
          break;
        }
        case TransactionPacket: {
          std::string receivedTransactions;
          std::vector<taraxa::bytes> transactions;
          auto transactionCount = _r.itemCount();
          for (auto iTransaction = 0; iTransaction < transactionCount;
               iTransaction++) {
            Transaction transaction(_r[iTransaction].data().toBytes());
            receivedTransactions += transaction.getHash().toString() + " ";
            peer->markTransactionAsKnown(transaction.getHash());
            transactions.emplace_back(_r[iTransaction].data().toBytes());
          }
          if (transactionCount > 0) {
            LOG(log_dg_trx_prp_) << "Received TransactionPacket with "
                                 << _r.itemCount() << " transactions";
            LOG(log_tr_trx_prp_)
                << "Received TransactionPacket with " << _r.itemCount()
                << " transactions:" << receivedTransactions.c_str();

            onNewTransactions(transactions, true);
          }
          break;
        }
        case PbftVotePacket: {
          LOG(log_dg_vote_prp_) << "In PbftVotePacket";

          Vote vote(_r[0].toBytes());
          LOG(log_dg_vote_prp_) << "Received PBFT vote " << vote.getHash();
          peer->markVoteAsKnown(vote.getHash());

          auto full_node = full_node_.lock();
          if (!full_node) {
            LOG(log_er_vote_prp_)
                << "Full node weak pointer empty in PbftVotePacket";
            return false;
          }

          if (full_node->addVote(vote)) {
            onNewPbftVote(vote);
          }

          break;
        }
        case GetPbftBlockPacket: {
          LOG(log_dg_pbft_sync_) << "Received GetPbftBlockPacket Block";
          auto full_node = full_node_.lock();
          if (full_node) {
            size_t height_to_sync = _r[0].toInt();
            size_t my_chain_size = full_node->getPbftChainSize();
            size_t blocks_to_transfer = 0;
            if (my_chain_size >= height_to_sync) {
              blocks_to_transfer =
                  std::min((uint64_t)conf_.network_sync_level_size,
                           (uint64_t)(my_chain_size - (height_to_sync - 1)));
            }
            LOG(log_dg_pbft_sync_) << "Send pbftblocks to " << _nodeID;
            sendPbftBlocks(_nodeID, height_to_sync, blocks_to_transfer);
          }
          break;
        }
        // no cert vote needed (propose block)
        case NewPbftBlockPacket: {
          LOG(log_dg_pbft_prp_) << "In NewPbftBlockPacket";

          PbftBlock pbft_block(_r[0]);
          uint64_t pbft_chain_size = _r[1].toInt();
          LOG(log_dg_pbft_prp_) << "Receive proposed PBFT Block " << pbft_block
                                << " Peer Chain size: " << pbft_chain_size;
          peer->markPbftBlockAsKnown(pbft_block.getBlockHash());
          if (pbft_chain_size > peer->pbft_chain_size_)
            peer->pbft_chain_size_ = pbft_chain_size;

          auto full_node = full_node_.lock();
          if (!full_node) {
            LOG(log_er_pbft_sync_) << "PbftBlock full node weak pointer empty";
            return false;
          }
          if (!full_node->isKnownUnverifiedPbftBlock(
                  pbft_block.getBlockHash())) {
            // TODO: need to check block validation, like proposed
            // vote(maybe
            //  come later), if get sortition etc
            full_node->pushUnverifiedPbftBlock(pbft_block);
            onNewPbftBlock(pbft_block);
          }
          break;
        }
        // need cert votes (syncing)
        case PbftBlockPacket: {
          auto item_count = _r.itemCount();
          LOG(log_dg_pbft_sync_)
              << "In PbftBlockPacket received " << item_count;
          auto full_node = full_node_.lock();
          if (!full_node) {
            LOG(log_er_pbft_sync_) << "PbftBlock full node weak pointer empty";
            return false;
          }
          uint64_t pbft_block_counter = 0;
          uint64_t dag_block_counter = 0;
          uint64_t dag_block_trx_counter = 0;

          pbft_sync_period_ = full_node->pbftSyncingPeriod();

          while (true) {
            if (pbft_block_counter + dag_block_counter +
                    dag_block_trx_counter >=
                item_count)
              break;
            PbftBlockCert pbft_blk_and_votes(
                _r[pbft_block_counter + dag_block_counter +
                   dag_block_trx_counter]
                    .data()
                    .toBytes());
            LOG(log_dg_pbft_sync_)
                << "Received pbft block: "
                << pbft_blk_and_votes.pbft_blk.getBlockHash();
            if (pbft_sync_period_ + 1 !=
                pbft_blk_and_votes.pbft_blk.getPeriod()) {
              LOG(log_er_pbft_sync_)
                  << "PBFT SYNC ERROR, UNEXPECTED PBFT BLOCK HEIGHT: "
                  << pbft_blk_and_votes.pbft_blk.getPeriod()
                  << " sync_period: " << pbft_sync_period_
                  << " chain size: " << full_node->getPbftChainSize()
                  << " queue: " << full_node->getPbftSyncedQueueSize();
              restartSyncingPbft(true);
              return true;
            }
            if (peer->pbft_chain_size_ <
                pbft_blk_and_votes.pbft_blk.getPeriod()) {
              peer->pbft_chain_size_ = pbft_blk_and_votes.pbft_blk.getPeriod();
            }

            pbft_block_counter++;
            std::string received_dag_blocks_str;
            std::map<uint64_t,
                     std::map<blk_hash_t,
                              std::pair<DagBlock, std::vector<Transaction>>>>
                dag_blocks_per_level;
            for (auto const &dag_hash :
                 pbft_blk_and_votes.pbft_blk.getSchedule().dag_blks_order) {
              DagBlock block(_r[pbft_block_counter + dag_block_counter +
                                dag_block_trx_counter]
                                 .data()
                                 .toBytes());
              dag_block_counter++;
              peer->markBlockAsKnown(block.getHash());

              std::vector<Transaction> newTransactions;
              for (auto const &trx_hash : block.getTrxs()) {
                Transaction transaction(
                    _r[pbft_block_counter + dag_block_counter +
                       dag_block_trx_counter]
                        .data()
                        .toBytes());
                dag_block_trx_counter++;
                newTransactions.push_back(transaction);
                peer->markTransactionAsKnown(transaction.getHash());
              }

              received_dag_blocks_str += block.getHash().toString() + " ";
              dag_blocks_per_level[block.getLevel()][block.getHash()] = {
                  block, newTransactions};
            }
            LOG(log_nf_dag_sync_)
                << "Received Dag Blocks: " << received_dag_blocks_str;
            for (auto block_level = dag_blocks_per_level.begin();
                 block_level != dag_blocks_per_level.end(); block_level++) {
              for (auto block : block_level->second) {
                auto status = checkTipsandPivot(block.second.first);
                if (!status.first) {
                  LOG(log_er_pbft_sync_)
                      << "PBFT SYNC ERROR, DAG missing a tip/pivot: "
                      << pbft_blk_and_votes.pbft_blk.getPeriod()
                      << " sync_period: " << pbft_sync_period_
                      << " chain size: " << full_node->getPbftChainSize()
                      << " queue: " << full_node->getPbftSyncedQueueSize();
                  restartSyncingPbft(true);
                  return true;
                }
                LOG(log_nf_dag_sync_)
                    << "Storing block "
                    << block.second.first.getHash().toString() << " with "
                    << block.second.second.size() << " transactions";
                if (block.second.first.getLevel() > peer->dag_level_)
                  peer->dag_level_ = block.second.first.getLevel();
                full_node->getBlockManager()->insertBroadcastedBlockWithTransactions(
                    block.second.first, block.second.second);
              }
            }

            auto pbft_blk_hash = pbft_blk_and_votes.pbft_blk.getBlockHash();
            peer->markPbftBlockAsKnown(pbft_blk_hash);

            // Check the PBFT block whether in the chain or in the synced
            // queue
            if (!full_node->isKnownPbftBlockForSyncing(pbft_blk_hash)) {
              // Check the PBFT block validation
              if (full_node->checkPbftBlockValidationFromSyncing(
                      pbft_blk_and_votes.pbft_blk)) {
                // Notice: cannot verify 2t+1 cert votes here. Since don't
                // have correct account status for nodes which after the
                // first synced one.
                full_node->setSyncedPbftBlock(pbft_blk_and_votes);
                pbft_sync_period_ = full_node->pbftSyncingPeriod();
                LOG(log_dg_pbft_sync_)
                    << "Receive synced PBFT block " << pbft_blk_and_votes;
              } else {
                LOG(log_wr_pbft_sync_) << "The PBFT block " << pbft_blk_hash
                                       << " failed validation. Drop it!";
              }
            }
          }
          if (item_count > 0) {
            if (syncing_ && peer_syncing_pbft == _nodeID) {
              if (pbft_sync_period_ >
                  full_node->getPbftChainSize() +
                      (10 * conf_.network_sync_level_size)) {
                LOG(log_dg_pbft_sync_)
                    << "Syncing pbft blocks faster than processing "
                    << pbft_sync_period_ << " "
                    << full_node->getPbftChainSize();
                host_.scheduleExecution(
                    1000, [this, _nodeID]() { delayedPbftSync(_nodeID, 1); });
              } else {
                syncPeerPbft(_nodeID, pbft_sync_period_ + 1);
              }
            } else {
              LOG(log_dg_pbft_sync_)
                  << "Received PbftBlockPacket from node " << _nodeID
                  << " but currently syncing with peer " << peer_syncing_pbft;
            }
          } else {
            LOG(log_dg_pbft_sync_) << "Syncing PBFT is completed";
            // We are pbft synced with the node we are connected to but
            // calling restartSyncingPbft will check if some nodes have
            // greater pbft chain size and we should continue syncing with
            // them
            restartSyncingPbft(true);
            // We are pbft synced, send message to other node to start gossiping
            // new blocks
            if (!syncing_ && !requesting_pending_dag_blocks_) {
              sendSyncedMessage();
            }
          }
          break;
        }
        case TestPacket:
          LOG(log_dg_) << "Received TestPacket";
          ++cnt_received_messages_[_nodeID];
          test_sums_[_nodeID] += _r[0].toInt();
          BOOST_ASSERT(_id == TestPacket);
          return (_id == TestPacket);
      };
    }
    return true;
  } catch (InvalidDataException &e) {
    LOG(log_er_) << "IvalidDataExcecption - Node will be disconnected "
                 << e.what();
  }
  return false;
}

void TaraxaCapability::delayedPbftSync(NodeID _nodeID, int counter) {
  if (!stopped_) {
    auto full_node = full_node_.lock();
    if (full_node) {
      if (counter > 60) {
        LOG(log_er_pbft_sync_)
            << "Pbft blocks stuck in queue, no new block processed "
               "in 60 seconds "
            << pbft_sync_period_ << " " << full_node->getPbftChainSize();
        syncing_ = false;
        LOG(log_dg_pbft_sync_) << "Syncing PBFT is stopping";
        return;
      }
      if (syncing_ && peer_syncing_pbft == _nodeID) {
        if (pbft_sync_period_ > full_node->getPbftChainSize() +
                                    (10 * conf_.network_sync_level_size)) {
          LOG(log_dg_pbft_sync_)
              << "Syncing pbft blocks faster than processing "
              << pbft_sync_period_ << " " << full_node->getPbftChainSize();
          host_.scheduleExecution(1000, [this, _nodeID, counter]() {
            delayedPbftSync(_nodeID, counter + 1);
          });
        } else {
          syncPeerPbft(_nodeID, pbft_sync_period_ + 1);
        }
      }
    }
  }
}

void TaraxaCapability::restartSyncingPbft(bool force) {
  if (syncing_ && !force) {
    LOG(log_dg_pbft_sync_)
        << "restartSyncingPbft called but syncing_ already true";
    return;
  }

  NodeID max_pbft_chain_nodeID;
  uint64_t max_pbft_chain_size = 0;
  uint64_t max_node_dag_level = 0;
  {
    boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
    for (auto const peer : peers_) {
      if (peer.second->pbft_chain_size_ > max_pbft_chain_size) {
        max_pbft_chain_size = peer.second->pbft_chain_size_;
        max_pbft_chain_nodeID = peer.first;
        max_node_dag_level = peer.second->dag_level_;
      } else if (peer.second->pbft_chain_size_ == max_pbft_chain_size &&
                 peer.second->dag_level_ > max_node_dag_level) {
        max_pbft_chain_nodeID = peer.first;
        max_node_dag_level = peer.second->dag_level_;
      }
    }
  }
  if (auto full_node = full_node_.lock()) {
    pbft_sync_period_ = full_node->pbftSyncingPeriod();

    if (max_pbft_chain_size > pbft_sync_period_) {
      if (!stopped_) {
        LOG(log_si_pbft_sync_)
            << "Restarting syncing PBFT" << max_pbft_chain_size << " "
            << pbft_sync_period_;
        requesting_pending_dag_blocks_ = false;
        syncing_ = true;
        peer_syncing_pbft = max_pbft_chain_nodeID;
        peer_syncing_pbft_chain_size_ = max_pbft_chain_size;
        syncPeerPbft(peer_syncing_pbft, pbft_sync_period_ + 1);
      }
    } else {
      LOG(log_nf_pbft_sync_)
          << "Restarting syncing PBFT not needed since our pbft chain "
             "size: "
          << pbft_sync_period_ << "(" << full_node->getPbftChainSize() << ")"
          << " is greater or equal than max node pbft chain size:"
          << max_pbft_chain_size;
      syncing_ = false;
      if (!requesting_pending_dag_blocks_ &&
          max_node_dag_level > full_node->getMaxDagLevelInQueue()) {
        LOG(log_nf_dag_sync_) << "Request pending " << max_node_dag_level << " "
                              << full_node->getMaxDagLevelInQueue() << "("
                              << full_node->getMaxDagLevel() << ")";
        requesting_pending_dag_blocks_ = true;
        requesting_pending_dag_blocks_node_id_ = max_pbft_chain_nodeID;
        requestPendingDagBlocks(max_pbft_chain_nodeID,
                                full_node->getMaxDagLevelInQueue() + 1);
      }
    }
  }
}

void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  LOG(log_nf_) << "Node " << _nodeID << " disconnected";
  cnt_received_messages_.erase(_nodeID);
  test_sums_.erase(_nodeID);
  erasePeer(_nodeID);
  if (syncing_ && peer_syncing_pbft == _nodeID && getPeersCount() > 0) {
    LOG(log_dg_pbft_sync_) << "Syncing PBFT is stopping";
    restartSyncingPbft(true);
  }
  if (requesting_pending_dag_blocks_ &&
      requesting_pending_dag_blocks_node_id_ == _nodeID) {
    requesting_pending_dag_blocks_ = false;
    restartSyncingPbft(true);
    if (!syncing_ && !requesting_pending_dag_blocks_) {
      sendSyncedMessage();
    }
  }
}

void TaraxaCapability::sendTestMessage(NodeID const &_id, int _x) {
  RLPStream s;
  host_.capabilityHost()->sealAndSend(
      _id, host_.capabilityHost()->prep(_id, name(), s, TestPacket, 1) << _x);
}

void TaraxaCapability::sendStatus(NodeID const &_id, bool _initial) {
  RLPStream s;
  if (auto full_node = full_node_.lock()) {
    if (_initial) {
      LOG(log_dg_) << "Sending initial status message to " << _id << " "
                   << c_protocolVersion << " " << conf_.network_id << " "
                   << full_node->getMaxDagLevel() << " " << genesis_ << " "
                   << full_node->getPbftChainSize();
    }
    LOG(log_dg_dag_sync_) << "Sending status message to " << _id
                          << " with dag level:" << full_node->getMaxDagLevel();
    LOG(log_dg_pbft_sync_) << "Sending status message to " << _id
                           << " with pbft chain size:"
                           << full_node->getPbftChainSize();
    if (_initial)
      host_.capabilityHost()->sealAndSend(
          _id, host_.capabilityHost()->prep(_id, name(), s, StatusPacket, 6)
                   << c_protocolVersion << conf_.network_id
                   << full_node->getMaxDagLevel() << genesis_
                   << full_node->getPbftChainSize() << syncing_);
    else {
      host_.capabilityHost()->sealAndSend(
          _id, host_.capabilityHost()->prep(_id, name(), s, StatusPacket, 3)
                   << full_node->getMaxDagLevel()
                   << full_node->getPbftChainSize() << syncing_);
    }
  }
}

vector<NodeID> TaraxaCapability::selectPeers(
    std::function<bool(TaraxaPeer const &)> const &_predicate) {
  vector<NodeID> allowed;
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  for (auto const &peer : peers_) {
    if (_predicate(*peer.second)) allowed.push_back(peer.first);
  }
  return allowed;
}

vector<NodeID> TaraxaCapability::getAllPeers() const {
  vector<NodeID> peers;
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  std::transform(
      peers_.begin(), peers_.end(), std::back_inserter(peers),
      [](std::pair<const dev::p2p::NodeID,
                   std::shared_ptr<taraxa::TaraxaPeer>> const &peer) {
        return peer.first;
      });
  return peers;
}

std::pair<std::vector<NodeID>, std::vector<NodeID>>
TaraxaCapability::randomPartitionPeers(std::vector<NodeID> const &_peers,
                                       std::size_t _number) {
  vector<NodeID> part1(_peers);
  vector<NodeID> part2;

  if (_number >= _peers.size()) return std::make_pair(part1, part2);

  std::shuffle(part1.begin(), part1.end(), urng_);

  // Remove elements from the end of the shuffled part1 vector and move
  // them to part2.
  std::move(part1.begin() + _number, part1.end(), std::back_inserter(part2));
  part1.erase(part1.begin() + _number, part1.end());
  return std::make_pair(move(part1), move(part2));
}

void TaraxaCapability::onNewTransactions(
    std::vector<taraxa::bytes> const &transactions, bool fromNetwork) {
  if (fromNetwork) {
    if (auto full_node = full_node_.lock()) {
      LOG(log_nf_trx_prp_) << "Storing " << transactions.size()
                           << " transactions";
      full_node->getTransactionManager()->insertBroadcastedTransactions(transactions);
    } else {
      for (auto const &transaction : transactions) {
        Transaction trx(transaction);
        auto trx_hash = trx.getHash();
        if (test_transactions_.find(trx_hash) == test_transactions_.end()) {
          test_transactions_[trx_hash] = trx;
          LOG(log_dg_trx_prp_) << "Received New Transaction " << trx_hash;
        } else {
          LOG(log_dg_trx_prp_) << "Received New Transaction" << trx_hash
                               << "that is already known";
        }
      }
    }
  }
  if (!fromNetwork || conf_.network_transaction_interval == 0) {
    {
      boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
      for (auto &peer : peers_) {
        if (!peer.second->syncing_) {
          std::vector<taraxa::bytes> transactionsToSend;
          std::vector<trx_hash_t> transactionsHashes;
          for (auto const &transaction : transactions) {
            Transaction trx(transaction);
            auto trx_hash = trx.getHash();
            if (!peer.second->isTransactionKnown(trx_hash)) {
              transactionsToSend.push_back(transaction);
              transactionsHashes.push_back(trx_hash);
            }
          }
          if (transactionsToSend.size() > 0) {
            sendTransactions(peer.first, transactionsToSend);
            for (auto &hash : transactionsHashes) {
              peer.second->markTransactionAsKnown(hash);
            }
          }
        }
      }
    }
  }
}

void TaraxaCapability::onNewBlockReceived(
    DagBlock block, std::vector<Transaction> transactions) {
  LOG(log_nf_dag_prp_) << "Receive DagBlock " << block.getHash() << " #Trx"
                       << transactions.size() << std::endl;
  if (auto full_node = full_node_.lock()) {
    if (full_node->getBlockManager()->isBlockKnown(block.getHash())) {
      LOG(log_dg_dag_prp_) << "Received NewBlock " << block.getHash().toString()
                           << "that is already known";
      return;
    } else {
      auto status = checkTipsandPivot(block);
      if (!status.first) {
        if (!syncing_ && !requesting_pending_dag_blocks_) restartSyncingPbft();
        return;
      }
      LOG(log_nf_dag_prp_) << "Storing block " << block.getHash().toString()
                           << " with " << transactions.size()
                           << " transactions";
      full_node->getBlockManager()->insertBroadcastedBlockWithTransactions(block, transactions);
    }
  } else if (test_blocks_.find(block.getHash()) == test_blocks_.end()) {
    test_blocks_[block.getHash()] = block;
    for (auto tr : transactions) {
      test_transactions_[tr.getHash()] = tr;
    }
    onNewBlockVerified(block);

  } else {
    LOG(log_dg_dag_prp_) << "Received NewBlock " << block.getHash().toString()
                         << "that is already known";
    return;
  }
}

void TaraxaCapability::sendSyncedMessage() {
  LOG(log_dg_dag_sync_) << "sendSyncedMessage ";
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  for (auto &peer : peers_) {
    RLPStream s;
    host_.capabilityHost()->prep(peer.first, name(), s, SyncedPacket, 0);
    host_.capabilityHost()->sealAndSend(peer.first, s);
  }
}

void TaraxaCapability::onNewBlockVerified(DagBlock const &block) {
  LOG(log_dg_dag_prp_) << "Verified NewBlock " << block.getHash().toString();
  verified_blocks_.insert(block.getHash());
  {
    std::unique_lock<std::mutex> lck(mtx_for_verified_blocks);
    condition_for_verified_blocks_.notify_all();
  }
  const int c_minBlockBroadcastPeers = 10;
  auto const peersWithoutBlock = selectPeers([&](TaraxaPeer const &_peer) {
    return !_peer.isBlockKnown(block.getHash());
  });

  auto const peersToSendNumber = std::max<std::size_t>(
      c_minBlockBroadcastPeers, std::sqrt(getPeersCount()));

  std::vector<NodeID> peersToSend;
  std::vector<NodeID> peersToAnnounce;
  std::tie(peersToSend, peersToAnnounce) =
      randomPartitionPeers(peersWithoutBlock, peersToSendNumber);

  for (NodeID const &peerID : peersToSend) {
    RLPStream ts;
    auto peer = getPeer(peerID);
    if (peer && !peer->syncing_) {
      sendBlock(peerID, block, true);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToSend.empty())
    LOG(log_dg_dag_prp_) << "Sent block to" << peersToSend.size() << " peers";

  for (NodeID const &peerID : peersToAnnounce) {
    RLPStream ts;
    auto peer = getPeer(peerID);
    if (peer && !peer->syncing_) {
      sendBlockHash(peerID, block);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToAnnounce.empty())
    LOG(log_dg_dag_prp_) << "Anounced block to " << peersToAnnounce.size()
                         << " peers";
}

void TaraxaCapability::sendBlocks(
    NodeID const &_id, std::vector<std::shared_ptr<DagBlock>> blocks) {
  RLPStream s;
  std::map<blk_hash_t, std::vector<taraxa::bytes>> blockTransactions;
  int totalTransactionsCount = 0;
  if (auto full_node = full_node_.lock()) {
    for (auto &block : blocks) {
      std::vector<taraxa::bytes> transactions;
      for (auto trx : block->getTrxs()) {
        auto t = full_node->getTransactionManager()->getTransaction(trx);
        if (!t) {
          if (!stopped_) {
            LOG(log_er_dag_sync_) << "Transacation " << trx
                                  << " is not available. SendBlocks canceled";
          }
          // TODO: This can happen on stopping the node because network
          // is not stopped since network does not support restart,
          // better solution needed
          return;
        }
        transactions.push_back(t->second);
        totalTransactionsCount++;
      }
      blockTransactions[block->getHash()] = transactions;
      LOG(log_nf_dag_sync_) << "Send DagBlock " << block->getHash()
                            << "# Trx: " << transactions.size() << std::endl;
    }
  }
  host_.capabilityHost()->prep(_id, name(), s, BlocksPacket,
                               blocks.size() + totalTransactionsCount);
  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
    taraxa::bytes trx_bytes;
    for (auto &trx : blockTransactions[block->getHash()]) {
      trx_bytes.insert(trx_bytes.end(), std::begin(trx), std::end(trx));
    }
    s.appendRaw(trx_bytes, blockTransactions[block->getHash()].size());
  }
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendLeavesBlocks(NodeID const &_id,
                                        std::vector<std::string> block_hashes) {
  RLPStream s;
  std::map<blk_hash_t, std::vector<taraxa::bytes>> blockTransactions;
  int totalTransactionsCount = 0;
  std::vector<std::shared_ptr<DagBlock>> blocks;
  if (auto full_node = full_node_.lock()) {
    for (auto &block_hash : block_hashes) {
      auto block = full_node->getDB()->getDagBlock(blk_hash_t(block_hash));
      if (block) {
        blocks.push_back(block);
        std::vector<taraxa::bytes> transactions;
        for (auto trx : block->getTrxs()) {
          auto t = full_node->getTransactionManager()->getTransaction(trx);
          if (!t) {
            if (!stopped_) {
              LOG(log_er_dag_sync_)
                  << "Transacation " << trx
                  << " is not available. sendLeavesBlocks canceled";
            }
            return;
          }
          transactions.push_back(t->second);
          totalTransactionsCount++;
        }
        blockTransactions[block->getHash()] = transactions;
        LOG(log_nf_dag_sync_) << "Send leaf DagBlock " << block->getHash()
                              << "# Trx: " << transactions.size() << std::endl;
      }
    }
  }
  host_.capabilityHost()->prep(_id, name(), s, LeavesBlocksPacket,
                               blocks.size() + totalTransactionsCount);
  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
    taraxa::bytes trx_bytes;
    for (auto &trx : blockTransactions[block->getHash()]) {
      trx_bytes.insert(trx_bytes.end(), std::begin(trx), std::end(trx));
    }
    s.appendRaw(trx_bytes, blockTransactions[block->getHash()].size());
  }
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendTransactions(
    NodeID const &_id, std::vector<taraxa::bytes> const &transactions) {
  LOG(log_nf_trx_prp_) << "sendTransactions" << transactions.size() << " to "
                       << _id;
  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, TransactionPacket,
                               transactions.size());
  taraxa::bytes trx_bytes;
  for (auto transaction : transactions) {
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction),
                     std::end(transaction));
  }
  s.appendRaw(trx_bytes, transactions.size());
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendBlock(NodeID const &_id, taraxa::DagBlock block,
                                 bool newBlock) {
  RLPStream s;
  vec_trx_t transactionsToSend;
  if (newBlock) {
    for (auto trx : block.getTrxs()) {
      auto peer = getPeer(_id);
      if (peer && !peer->isTransactionKnown(trx))
        transactionsToSend.push_back(trx);
    }
    host_.capabilityHost()->prep(_id, name(), s, NewBlockPacket,
                                 1 + transactionsToSend.size());
  } else {
    host_.capabilityHost()->prep(_id, name(), s, BlockPacket,
                                 1 + block.getTrxs().size());
    transactionsToSend = block.getTrxs();
  }
  s.appendRaw(block.rlp(true));

  taraxa::bytes trx_bytes;
  for (auto trx : transactionsToSend) {
    std::shared_ptr<std::pair<Transaction, taraxa::bytes>> transaction;
    if (auto full_node = full_node_.lock()) {
      transaction = full_node->getTransactionManager()->getTransaction(trx);
    } else {
      assert(test_transactions_.find(trx) != test_transactions_.end());
      transaction = std::make_shared<std::pair<Transaction, taraxa::bytes>>(
          test_transactions_[trx], test_transactions_[trx].rlp(true));
    }
    assert(transaction != nullptr);  // We should never try to send a block for
                                     // which  we do not have all transactions
    trx_bytes.insert(trx_bytes.end(), std::begin(transaction->second),
                     std::end(transaction->second));
  }
  s.appendRaw(trx_bytes, transactionsToSend.size());
  host_.capabilityHost()->sealAndSend(_id, s);
  if (newBlock)
    LOG(log_dg_dag_prp_) << "Send DagBlock " << block.getHash()
                         << " #Trx: " << transactionsToSend.size() << std::endl;
  else
    LOG(log_dg_dag_sync_) << "Send DagBlock " << block.getHash()
                          << " newBlock? " << newBlock
                          << " #Trx: " << transactionsToSend.size()
                          << std::endl;
}

void TaraxaCapability::sendBlockHash(NodeID const &_id,
                                     taraxa::DagBlock block) {
  LOG(log_dg_dag_prp_) << "sendBlockHash " << block.getHash().toString();
  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, NewBlockHashPacket, 1);
  s.append(block.getHash());
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlock(NodeID const &_id, blk_hash_t hash,
                                    bool newBlock) {
  if (newBlock)
    LOG(log_dg_dag_prp_) << "requestBlock " << hash.toString();
  else
    LOG(log_dg_dag_sync_) << "requestBlock " << hash.toString();
  RLPStream s;
  if (newBlock)
    host_.capabilityHost()->prep(_id, name(), s, GetNewBlockPacket, 1);
  else
    host_.capabilityHost()->prep(_id, name(), s, GetBlockPacket, 1);
  s.append(hash);
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestPbftBlocks(NodeID const &_id,
                                         size_t height_to_sync) {
  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, GetPbftBlockPacket, 1);
  s << height_to_sync;
  LOG(log_dg_pbft_sync_) << "Sending GetPbftBlockPacket with height: "
                         << height_to_sync;
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestPendingDagBlocks(NodeID const &_id,
                                               level_t level) {
  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, GetBlocksPacket, 1);
  s << level;
  LOG(log_nf_dag_sync_) << "Sending GetBlocksPacket";
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestLeavesDagBlocks(NodeID const &_id) {
  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, GetLeavesBlocksPacket, 0);
  LOG(log_nf_dag_sync_) << "Sending GetLeavesBlocksPacket";
  host_.capabilityHost()->sealAndSend(_id, s);
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

std::map<blk_hash_t, taraxa::DagBlock> TaraxaCapability::getBlocks() {
  return test_blocks_;
}

std::map<trx_hash_t, taraxa::Transaction> TaraxaCapability::getTransactions() {
  return test_transactions_;
}

void TaraxaCapability::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
}

void TaraxaCapability::sendTransactions() {
  auto full_node = full_node_.lock();
  if (full_node) {
    onNewTransactions(full_node->getNewVerifiedTrxSnapShotSerialized(), false);
  }
  host_.scheduleExecution(conf_.network_transaction_interval,
                          [this]() { sendTransactions(); });
}

void TaraxaCapability::doBackgroundWork() {
  auto full_node = full_node_.lock();
  for (auto const &peer : peers_) {
    // Disconnect any node that did not send any message for 3 status
    // intervals
    if (!peer.second->checkStatus(3)) {
      LOG(log_nf_) << "Host disconnected, no status message in "
                   << 3 * check_status_interval_ << " seconds" << peer.first;
      host_.capabilityHost()->disconnect(peer.first, p2p::PingTimeout);
    }
    // Send status message
    else {
      sendStatus(peer.first, false);
    }
  }
  host_.scheduleExecution(check_status_interval_,
                          [this]() { doBackgroundWork(); });
}

void TaraxaCapability::onStarting() {
  auto full_node = full_node_.lock();
  if (conf_.network_simulated_delay > 0) {
    const int number_of_delayed_threads = 5;
    io_work_ = std::make_shared<boost::asio::io_service::work>(io_service_);
    for (int i = 0; i < number_of_delayed_threads; ++i)
      delay_threads_.create_thread([&]() { io_service_.run(); });
  }
  if (conf_.network_transaction_interval > 0)
    host_.scheduleExecution(conf_.network_transaction_interval,
                            [this]() { sendTransactions(); });
  if (full_node) {
    check_status_interval_ =
        6 * full_node->getConfig().test_params.pbft.lambda_ms_min;
    host_.scheduleExecution(check_status_interval_,
                            [this]() { doBackgroundWork(); });
  }
}

void TaraxaCapability::onNewPbftVote(taraxa::Vote const &vote) {
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  for (auto const &peer : peers_) {
    if (!peer.second->isVoteKnown(vote.getHash())) {
      sendPbftVote(peer.first, vote);
    }
  }
}

void TaraxaCapability::sendPbftVote(NodeID const &_id,
                                    taraxa::Vote const &vote) {
  LOG(log_dg_vote_prp_) << "sendPbftVote " << vote.getHash() << " to " << _id;
  auto vote_rlp = vote.rlp();

  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, PbftVotePacket, 1);
  s.append(vote_rlp);
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::onNewPbftBlock(taraxa::PbftBlock const &pbft_block) {
  auto full_node = full_node_.lock();
  if (full_node) {
    auto my_chain_size = full_node->getPbftChainSize();
    boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
    for (auto const &peer : peers_) {
      if (!peer.second->isPbftBlockKnown(pbft_block.getBlockHash())) {
        if (!peer.second->syncing_) {
          sendPbftBlock(peer.first, pbft_block, my_chain_size);
        }
      }
    }
  }
}

// api for pbft syncing
void TaraxaCapability::sendPbftBlocks(NodeID const &_id, size_t height_to_sync,
                                      size_t blocks_to_transfer) {
  LOG(log_dg_pbft_sync_)
      << "In sendPbftBlocks, peer want to sync from pbft chain height "
      << height_to_sync << ", will send " << blocks_to_transfer
      << " pbft blocks to " << _id;
  if (auto full_node = full_node_.lock()) {
    auto pbftchain = full_node->getPbftChain();
    assert(pbftchain);
    auto db_ = full_node->getDB();
    assert(db_);
    // add cert votes for each pbftblock
    std::vector<PbftBlock> blocks =
        pbftchain->getPbftBlocks(height_to_sync, blocks_to_transfer);
    std::vector<PbftBlockCert> cert_blocks;
    std::map<blk_hash_t, std::vector<std::shared_ptr<DagBlock>>>
        pbft_dag_blocks;
    uint64_t dag_blocks_counter = 0;
    uint64_t dag_blocks_transaction_counter = 0;
    // has some redundancy here. fix later
    for (auto const &b : blocks) {
      auto cert_votes_rlp = db_->getVote(b.getBlockHash());
      PbftBlockCert bk(b, cert_votes_rlp);
      cert_blocks.emplace_back(bk);
      for (auto const &hash : b.getSchedule().dag_blks_order) {
        auto dag_block = full_node->getDB()->getDagBlock(hash);
        pbft_dag_blocks[b.getBlockHash()].push_back(dag_block);
        dag_blocks_counter++;
        dag_blocks_transaction_counter += dag_block->getTrxs().size();
      }
    }

    RLPStream s;
    host_.capabilityHost()->prep(_id, name(), s, PbftBlockPacket,
                                 cert_blocks.size() + dag_blocks_counter +
                                     dag_blocks_transaction_counter);
    for (auto const &b : cert_blocks) {
      s.appendRaw(b.rlp());
      for (auto const &dag_block : pbft_dag_blocks[b.pbft_blk.getBlockHash()]) {
        s.appendRaw(dag_block->rlp(true));
        for (auto const &trx_hash : dag_block->getTrxs()) {
          s.appendRaw(full_node->getTransactionManager()->getTransaction(trx_hash)->second);
        }
      }
    }
    host_.capabilityHost()->sealAndSend(_id, s);
    // Question: will send multiple times to a same receiver, why?
    LOG(log_dg_pbft_sync_) << "Sending PbftCertBlocks " << cert_blocks << " to "
                           << _id;
  }
}

void TaraxaCapability::sendPbftBlock(NodeID const &_id,
                                     taraxa::PbftBlock const &pbft_block,
                                     uint64_t const &pbft_chain_size) {
  LOG(log_dg_pbft_prp_) << "sendPbftBlock " << pbft_block.getBlockHash()
                        << " to " << _id;

  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, NewPbftBlockPacket, 2);
  pbft_block.streamRLP(s, true);
  s << pbft_chain_size;
  host_.capabilityHost()->sealAndSend(_id, s);
}

Json::Value TaraxaCapability::getStatus() const {
  Json::Value res;
  res["synced"] = Json::UInt64(!this->syncing_);
  res["peers"] = Json::Value(Json::arrayValue);
  boost::unique_lock<boost::shared_mutex> lock(peers_mutex_);
  for (auto &peer : peers_) {
    Json::Value peer_status;
    peer_status["node_id"] = peer.first.toString();
    peer_status["dag_level"] = Json::UInt64(peer.second->dag_level_);
    peer_status["pbft_size"] = Json::UInt64(peer.second->pbft_chain_size_);
    peer_status["dag_synced"] = !peer.second->syncing_;
    res["peers"].append(peer_status);
  }
  return res;
}

std::string TaraxaCapability::packetToPacketName(byte const &packet) {
  switch (packet) {
    case StatusPacket:
      return "StatusPacket";
    case NewBlockPacket:
      return "NewBlockPacket";
    case BlockPacket:
      return "BlockPacket";
    case NewBlockHashPacket:
      return "NewBlockHashPacket";
    case GetNewBlockPacket:
      return "GetNewBlockPacket";
    case GetBlockPacket:
      return "GetBlockPacket";
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
    case NewPbftBlockPacket:
      return "NewPbftBlockPacket";
    case GetPbftBlockPacket:
      return "GetPbftBlockPacket";
    case PbftBlockPacket:
      return "PbftBlockPacket";
    case PacketCount:
      return "PacketCount";
  }
  return std::to_string(packet);
}
