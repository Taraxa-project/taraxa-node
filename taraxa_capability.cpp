#include "taraxa_capability.hpp"
#include "dag.hpp"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "vote.h"

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

void TaraxaCapability::syncPeer(NodeID const &_nodeID,
                                unsigned long level_to_sync) {
  if (auto full_node = full_node_.lock()) {
    LOG(log_nf_) << "Sync Peer:" << _nodeID;
    auto peer = getPeer(_nodeID);
    if (level_to_sync == 0) level_to_sync = 1;
    requestBlocksLevel(_nodeID, level_to_sync, conf_.network_sync_level_size);
  }
}

void TaraxaCapability::syncPeerPbft(NodeID const &_nodeID) {
  if (auto full_node = full_node_.lock()) {
    size_t height_to_sync = full_node->getPbftChainSize() + 1;
    LOG(log_nf_) << "Sync peer node " << _nodeID << " from pbft chain height "
                 << height_to_sync;
    requestPbftBlocks(_nodeID, height_to_sync);
  }
}

std::pair<bool, blk_hash_t> TaraxaCapability::checkTipsandPivot(
    DagBlock const &block) {
  if (auto full_node = full_node_.lock()) {
    for (auto const &tip : block.getTips()) {
      if (!full_node->isBlockKnown(tip)) {
        LOG(log_nf_) << "Block " << block.getHash().toString()
                     << " has a missing tip " << tip.toString();
        return std::make_pair(false, tip);
      }
    }
    auto pivot = block.getPivot();
    if (!full_node->isBlockKnown(pivot)) {
      LOG(log_nf_) << "Block " << block.getHash().toString()
                   << " has a missing pivot " << pivot.toString();
      return std::make_pair(false, pivot);
    }
  }
  return std::make_pair(true, blk_hash_t());
}

void TaraxaCapability::continueSync(NodeID const &_nodeID) {
  if (auto full_node = full_node_.lock()) {
    auto peer = getPeer(_nodeID);
    int max_block_level_received = 0;
    if (peer) {
      for (auto block_level = peer->m_syncBlocks.begin();
           block_level != peer->m_syncBlocks.end();) {
        for (auto block : block_level->second) {
          if (block.second.first.getLevel() > max_block_level_received)
            max_block_level_received = block.second.first.getLevel();
          auto status = checkTipsandPivot(block.second.first);
          if (!status.first) {
            peer->m_lastRequest = status.second;
            requestBlock(_nodeID, status.second, false);
            return;
          }
        }

        for (auto block : block_level->second) {
          LOG(log_nf_) << "Storing block "
                       << block.second.first.getHash().toString() << " with "
                       << block.second.second.size() << " transactions";
          full_node->insertBroadcastedBlockWithTransactions(
              block.second.first, block.second.second);
        }
        peer->m_syncBlocks.erase(block_level++);
      }
      peer->m_syncBlocks.clear();
      if (syncing_ && peer_syncing_ == _nodeID) {
        uint64_t max_level = full_node->getMaxDagLevel();
        while (max_block_level_received >
               max_level + (10 * conf_.network_sync_level_size)) {
          LOG(log_er_) << "Syncing blocks faster then processing"
                       << max_block_level_received << " " << max_level;
          thisThreadSleepForSeconds(1);
          max_level = full_node->getMaxDagLevel();
        }
        if (max_block_level_received < max_level) {
          syncPeer(_nodeID, max_level + 1);
        } else {
          syncPeer(_nodeID, max_block_level_received + 1);
        }
      }
    }
  }
}

void TaraxaCapability::onConnect(NodeID const &_nodeID, u256 const &) {
  LOG(log_nf_) << "Node " << _nodeID << " connected";
  cnt_received_messages_[_nodeID] = 0;
  test_sums_[_nodeID] = 0;

  insertPeer(_nodeID, std::make_shared<TaraxaPeer>(_nodeID));
  sendStatus(_nodeID);
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

bool TaraxaCapability::interpretCapabilityPacketImpl(NodeID const &_nodeID,
                                                     unsigned _id,
                                                     RLP const &_r) {
  auto peer = getPeer(_nodeID);
  if (peer) {
    peer->setAsking(false);
    peer->setLastMessage();
    switch (_id) {
      case SyncedPacket: {
        LOG(log_dg_) << "Received synced message from " << _nodeID;
        peer->syncing_ = false;
      } break;
      case StatusPacket: {
        auto const peer_protocol_version = _r[0].toInt<unsigned>();
        auto const network_id = _r[1].toString();
        auto const level = _r[2].toInt();
        auto const genesis_hash = _r[3].toString();
        LOG(log_dg_) << "Received status message from " << _nodeID << " "
                     << peer_protocol_version << " " << network_id << " "
                     << level << " " << genesis_;

        if (peer_protocol_version != c_protocolVersion) {
          LOG(log_er_) << "Incorrect protocol version " << peer_protocol_version
                       << ", host " << _nodeID << " will be disconnected";
          host_.capabilityHost()->disconnect(_nodeID, p2p::UserReason);
        }
        if (network_id != conf_.network_id) {
          LOG(log_er_) << "Incorrect network id " << network_id << ", host "
                       << _nodeID << " will be disconnected";
          host_.capabilityHost()->disconnect(_nodeID, p2p::UserReason);
        }
        if (genesis_hash != genesis_) {
          LOG(log_er_) << "Incorrect genesis hash " << genesis_hash << ", host "
                       << _nodeID << " will be disconnected";
          host_.capabilityHost()->disconnect(_nodeID, p2p::UserReason);
        }
        if (auto full_node = full_node_.lock()) {
          int max_level = full_node->getMaxDagLevel();
          peer->level_ = level;
          if (level > max_peer_level_) {
            max_peer_level_ = level;
            // If our block level is less then max peer level start sync
            if (max_level < max_peer_level_) {
              syncing_ = true;
              peer_syncing_ = _nodeID;
              syncPeer(_nodeID, max_level);
            }
          }
          // Why PBFT chain need sync here?
          // syncPeerPbft(_nodeID);
          // Start gossiping if other node is within 10 blocks from our level
          peer->syncing_ = level > (max_level - 10);
        }
        break;
      }
      // Means a new block is proposed, full block body and all transaction
      // are received.
      case NewBlockPacket: {
        DagBlock block(_r[0].data().toBytes());

        auto transactionsCount = _r.itemCount() - 1;
        LOG(log_dg_) << "Received NewBlockPacket " << transactionsCount;

        std::vector<Transaction> newTransactions;
        for (auto iTransaction = 1; iTransaction < transactionsCount + 1;
             iTransaction++) {
          Transaction transaction(_r[iTransaction].data().toBytes());
          newTransactions.push_back(transaction);
          peer->markTransactionAsKnown(transaction.getHash());
        }

        peer->markBlockAsKnown(block.getHash());
        onNewBlockReceived(block, newTransactions);
        break;
      }
      // Full block and partial transactions are received
      case BlockPacket: {
        DagBlock block(_r[0].data().toBytes());

        auto transactionsCount = _r.itemCount() - 1;
        std::unordered_map<trx_hash_t, Transaction> newTransactions;
        std::vector<Transaction> vec_new_trxs;
        for (auto iTransaction = 1; iTransaction < transactionsCount + 1;
             iTransaction++) {
          Transaction transaction(_r[iTransaction].data().toBytes());
          newTransactions[transaction.getHash()] = transaction;
          vec_new_trxs.emplace_back(transaction);
          peer->markTransactionAsKnown(transaction.getHash());
        }

        LOG(log_dg_) << "Received BlockPacket " << block.getHash().toString();
        peer->markBlockAsKnown(block.getHash());
        // Initial syncing
        if (peer->m_lastRequest == block.getHash()) {
          std::vector<Transaction> vTransactions;
          for (const auto &t : newTransactions)
            vTransactions.push_back(t.second);
          peer->m_syncBlocks[block.getLevel()][block.getHash()] = {
              block, vTransactions};
          continueSync(_nodeID);
        } else if (auto full_node = full_node_.lock()) {
          auto status = checkTipsandPivot(block);
          if (!status.first) {
            restartSyncing();
            break;
          }
          LOG(log_nf_) << "Storing blocks " << block.getHash().toString()
                       << " with transactions " << vec_new_trxs.size();
          full_node->insertBroadcastedBlockWithTransactions(block,
                                                            vec_new_trxs);
        } else {
          for (const auto &transaction : newTransactions) {
            if (test_transactions_.find(transaction.first) ==
                test_transactions_.end()) {
              test_transactions_[transaction.first] = transaction.second;
              LOG(log_dg_) << "Received New Transaction "
                           << transaction.first.toString();
            } else {
              LOG(log_dg_) << "Received New Transaction"
                           << transaction.first.toString()
                           << "that is already known";
            }
          }
          test_blocks_[block.getHash()] = block;
        }
      } break;
      case NewBlockHashPacket: {
        blk_hash_t hash(_r[0]);
        LOG(log_dg_) << "Received NewBlockHashPacket" << hash.toString();
        peer->markBlockAsKnown(hash);
        if (auto full_node = full_node_.lock()) {
          if (!full_node->isBlockKnown(hash) &&
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
        LOG(log_dg_) << "Received GetBlockPacket" << hash.toString();
        peer->markBlockAsKnown(hash);
        if (auto full_node = full_node_.lock()) {
          auto block = full_node->getDagBlockFromDb(hash);
          if (block) {
            sendBlock(_nodeID, *block, false);
          } else
            LOG(log_nf_) << "NO PACKET: " << hash.toString();
        }
        break;
      }
      case GetNewBlockPacket: {
        blk_hash_t hash(_r[0]);
        peer->markBlockAsKnown(hash);
        LOG(log_dg_) << "Received GetNewBlockPacket" << hash.toString();

        if (auto full_node = full_node_.lock()) {
          auto block = full_node->getDagBlockFromDb(hash);
          if (block) {
            sendBlock(_nodeID, *block, true);
          } else
            LOG(log_nf_) << "NO NEW PACKET: " << hash.toString();
        } else if (test_blocks_.find(hash) != test_blocks_.end()) {
          sendBlock(_nodeID, test_blocks_[hash], true);
        }
        break;
      }
      case GetBlocksLevelPacket: {
        auto level = _r[0].toInt();
        auto number_of_levels = _r[1].toInt();
        LOG(log_dg_) << "Received GetBlocksLevelPacket with level " << level
                     << " " << number_of_levels;

        if (auto full_node = full_node_.lock()) {
          auto blocks = full_node->getDagBlocksAtLevel(level, number_of_levels);
          sendBlocks(_nodeID, blocks);
        }
        break;
      }
      case BlocksPacket: {
        std::string receivedBlocks;
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

          receivedBlocks += block.getHash().toString() + " ";
          peer->m_syncBlocks[block.getLevel()][block.getHash()] = {
              block, newTransactions};
          if (iBlock + transactionCount + 1 >= itemCount) break;
        }
        if (itemCount > 0) {
          LOG(log_nf_) << "Received BlocksPacket with " << receivedBlocks.size()
                       << "  blocks:" << receivedBlocks.c_str();
          continueSync(_nodeID);
        } else {
          // We are synced, send message to other node to start gossiping new
          // blocks
          if (syncing_) {
            syncing_ = false;
            sendSyncedMessage();
            continueSync(_nodeID);
            // Call continue Sync, just one more time to make sure that no block
            // is missed between stopping sync and starting gossiping
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
          LOG(log_dg_) << "Received TransactionPacket with " << _r.itemCount()
                       << " transactions:" << receivedTransactions.c_str();
          onNewTransactions(transactions, true);
        }
        break;
      }
      case PbftVotePacket: {
        LOG(log_dg_) << "In PbftVotePacket";

        std::vector<::byte> pbft_vote_bytes;

        for (auto i = 0; i < _r[0].itemCount(); i++) {
          pbft_vote_bytes.push_back(_r[0][i].toInt());
        }
        taraxa::bufferstream strm(pbft_vote_bytes.data(),
                                  pbft_vote_bytes.size());
        Vote vote;
        vote.deserialize(strm);
        LOG(log_dg_) << "Received PBFT vote " << vote.getHash();

        peer->markVoteAsKnown(vote.getHash());

        auto full_node = full_node_.lock();
        if (!full_node) {
          LOG(log_er_) << "Full node weak pointer empty in PbftVotePacket";
          return false;
        }

        if (full_node->addVote(vote)) {
          onNewPbftVote(vote);
        }

        break;
      }
      case GetPbftBlockPacket: {
        LOG(log_dg_) << "Received GetPbftBlockPacket Block";
        // TODO: Since syncing PBFT block and cert votes validation issue, each
        //  time sync one block. And need to fix later
        const size_t max_blocks_in_packet = 1;
        auto full_node = full_node_.lock();
        if (full_node) {
          size_t height_to_sync = _r[0].toInt();
          size_t my_chain_size = full_node->getPbftChainSize();
          if (my_chain_size >= height_to_sync) {
            size_t blocks_to_transfer = std::min(
                max_blocks_in_packet, my_chain_size - (height_to_sync - 1));
            // Question: will send multiple times to same receiver, why?
            LOG(log_dg_) << "Send pbftblocks to " << _nodeID;
            sendPbftBlocks(_nodeID, height_to_sync, blocks_to_transfer);
          }
        }
        break;
      }
      // no cert vote needed (propose block)
      case NewPbftBlockPacket: {
        LOG(log_dg_) << "In NewPbftBlockPacket";

        PbftBlock pbft_block(_r[0]);
        LOG(log_dg_) << "Received PBFT Block " << pbft_block.getBlockHash();
        peer->markPbftBlockAsKnown(pbft_block.getBlockHash());

        auto full_node = full_node_.lock();
        if (!full_node) {
          LOG(log_er_) << "PbftBlock full node weak pointer empty";
          return false;
        }
        if (!full_node->isKnownUnverifiedPbftBlock(pbft_block.getBlockHash())) {
          // TODO: need to check block validation, like proposed vote(maybe
          //  come later), if get sortition etc
          full_node->pushUnverifiedPbftBlock(pbft_block);
          onNewPbftBlock(pbft_block);
        }
        break;
      }
      // need cert votes (syncing)
      case PbftBlockPacket: {
        LOG(log_dg_) << "In PbftBlockPacket";

        auto block_count = _r.itemCount();
        for (auto iblock = 0; iblock < block_count; iblock++) {
          PbftBlockCert blk_and_votes(_r[iblock].toBytes());
          auto pbft_blk_hash = blk_and_votes.pbft_blk.getBlockHash();
          peer->markPbftBlockAsKnown(pbft_blk_hash);
          auto full_node = full_node_.lock();
          if (!full_node) {
            LOG(log_er_) << "PbftBlock full node weak pointer empty";
            return false;
          }
          if (!full_node->isKnownPbftBlockForSyncing(pbft_blk_hash)) {
            if (full_node->checkPbftBlockValidation(blk_and_votes.pbft_blk)) {
              if (full_node->pbftBlockHasEnoughCertVotes(pbft_blk_hash, blk_and_votes.cert_votes)) {
                // Check 2t+1 cert votes, then put PBFT block into chain and
                //  store cert votes in DB.
                full_node->setVerifiedPbftBlock(blk_and_votes.pbft_blk);
                full_node->storeCertVotes(pbft_blk_hash,
                                          blk_and_votes.cert_votes);
                LOG(log_dg_) << "Pbftblock " << pbft_blk_hash
                             << " have enough cert votes!";
              } else {
                LOG(log_wr_) << "Pbftblock " << pbft_blk_hash
                             << " does not have enough valid cert votes";
              }
            }
          }
        }
        // TODO: Since the received PBFT block is not put into chain immediately
        //  , each time will send the same height to ask syncing. Need fix
        // if (block_count > 0) {
        //   syncPeerPbft(_nodeID);
        // }
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
}

void TaraxaCapability::restartSyncing() {
  if (syncing_) return;
  LOG(log_nf_) << "Restarting syncing";
  NodeID max_level_nodeID;
  unsigned long max_level = 0;
  {
    boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
    for (auto const peer : peers_) {
      if (peer.second->level_ > max_level) {
        max_level = peer.second->level_;
        max_level_nodeID = peer.first;
      }
    }
  }
  if (auto full_node = full_node_.lock()) {
    if (!stopped_) {
      syncing_ = true;
      peer_syncing_ = max_level_nodeID;
      syncPeer(max_level_nodeID, full_node->getMaxDagLevel());
      // Why PBFT chain need sync here?
      // syncPeerPbft(max_level_nodeID);
    }
  }
}

void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  LOG(log_nf_) << "Node " << _nodeID << " disconnected";
  cnt_received_messages_.erase(_nodeID);
  test_sums_.erase(_nodeID);
  // If syncing to the disconnected peer, find another peer to sync with
  if (syncing_ && peer_syncing_ == _nodeID && getPeersCount() > 0) {
    restartSyncing();
  }
  erasePeer(_nodeID);
}

void TaraxaCapability::sendTestMessage(NodeID const &_id, int _x) {
  RLPStream s;
  host_.capabilityHost()->sealAndSend(
      _id, host_.capabilityHost()->prep(_id, name(), s, TestPacket, 1) << _x);
}

void TaraxaCapability::sendStatus(NodeID const &_id) {
  RLPStream s;
  if (auto full_node = full_node_.lock()) {
    LOG(log_dg_) << "Sending status message to " << _id << " "
                 << c_protocolVersion << " " << conf_.network_id << " "
                 << full_node->getMaxDagLevel() << " "
                 << full_node->getMaxDagLevel() << genesis_;
    host_.capabilityHost()->sealAndSend(
        _id, host_.capabilityHost()->prep(_id, name(), s, StatusPacket, 4)
                 << c_protocolVersion << conf_.network_id
                 << full_node->getMaxDagLevel() << genesis_);
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
  for (auto const &peer : peers_) {
    peers.push_back(peer.first);
  }
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
      LOG(log_nf_) << "Storing " << transactions.size() << " transactions";
      full_node->insertBroadcastedTransactions(transactions);
    } else {
      for (auto const &transaction : transactions) {
        Transaction trx(transaction);
        auto trx_hash = trx.getHash();
        if (test_transactions_.find(trx_hash) == test_transactions_.end()) {
          test_transactions_[trx_hash] = trx;
          LOG(log_dg_) << "Received New Transaction " << trx_hash;
        } else {
          LOG(log_dg_) << "Received New Transaction" << trx_hash
                       << "that is already known";
        }
      }
    }
  }
  if (!fromNetwork || conf_.network_transaction_interval == 0) {
    std::map<NodeID, std::vector<taraxa::bytes>> transactionsToSend;
    {
      boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
      for (auto &peer : peers_) {
        if (!peer.second->syncing_) {
          for (auto const &transaction : transactions) {
            Transaction trx(transaction);
            auto trx_hash = trx.getHash();
            if (!peer.second->isTransactionKnown(trx_hash)) {
              peer.second->markTransactionAsKnown(trx_hash);
              transactionsToSend[peer.first].push_back(transaction);
            }
          }
        }
      }
    }
    for (auto &t : transactionsToSend) {
      if (t.second.size() > 0) {
        sendTransactions(t.first, t.second);
      }
    }
  }
}

void TaraxaCapability::onNewBlockReceived(
    DagBlock block, std::vector<Transaction> transactions) {
  if (auto full_node = full_node_.lock()) {
    if (full_node->isBlockKnown(block.getHash())) {
      LOG(log_dg_) << "Received NewBlock " << block.getHash().toString()
                   << "that is already known";
      return;
    } else {
      auto status = checkTipsandPivot(block);
      if (!status.first) {
        restartSyncing();
        return;
      }
      LOG(log_nf_) << "Storing block " << block.getHash().toString() << " with "
                   << transactions.size() << " transactions";
      full_node->insertBroadcastedBlockWithTransactions(block, transactions);
    }
  } else if (test_blocks_.find(block.getHash()) == test_blocks_.end()) {
    test_blocks_[block.getHash()] = block;
    for (auto tr : transactions) {
      test_transactions_[tr.getHash()] = tr;
    }
    onNewBlockVerified(block);

  } else {
    LOG(log_dg_) << "Received NewBlock " << block.getHash().toString()
                 << "that is already known";
    return;
  }
}

void TaraxaCapability::sendSyncedMessage() {
  LOG(log_dg_) << "sendSyncedMessage ";
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  for (auto &peer : peers_) {
    RLPStream s;
    host_.capabilityHost()->prep(peer.first, name(), s, SyncedPacket, 0);
    host_.capabilityHost()->sealAndSend(peer.first, s);
  }
}

void TaraxaCapability::onNewBlockVerified(DagBlock block) {
  LOG(log_dg_) << "Verified NewBlock " << block.getHash().toString();
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
    LOG(log_dg_) << "Sent block to" << peersToSend.size() << " peers";

  for (NodeID const &peerID : peersToAnnounce) {
    RLPStream ts;
    auto peer = getPeer(peerID);
    if (peer && !peer->syncing_) {
      sendBlockHash(peerID, block);
      peer->markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToAnnounce.empty())
    LOG(log_dg_) << "Anounced block to " << peersToAnnounce.size() << " peers";
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
        auto t = full_node->getTransaction(trx);
        if (!t) {
          if (!stopped_) {
            LOG(log_er_) << "Transacation " << trx
                         << " is not available. SendBlocks canceled";
          }
          // TODO: This can happen on stopping the node because network is not
          // stopped since network does not support restart, better solution
          // needed
          return;
        }
        transactions.push_back(t->second);
        totalTransactionsCount++;
      }
      blockTransactions[block->getHash()] = transactions;
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

void TaraxaCapability::sendTransactions(
    NodeID const &_id, std::vector<taraxa::bytes> const &transactions) {
  LOG(log_dg_) << "sendTransactions" << transactions.size() << " to " << _id;
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
  LOG(log_dg_) << "sendBlock " << block.getHash().toString();
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
      transaction = full_node->getTransaction(trx);
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
}

void TaraxaCapability::sendBlockHash(NodeID const &_id,
                                     taraxa::DagBlock block) {
  LOG(log_dg_) << "sendBlockHash " << block.getHash().toString();
  RLPStream s;
  std::vector<uint8_t> bytes;
  host_.capabilityHost()->prep(_id, name(), s, NewBlockHashPacket, 1);
  s.append(block.getHash());
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlock(NodeID const &_id, blk_hash_t hash,
                                    bool newBlock) {
  LOG(log_dg_) << "requestBlock " << hash.toString();
  RLPStream s;
  std::vector<uint8_t> bytes;
  if (newBlock)
    host_.capabilityHost()->prep(_id, name(), s, GetNewBlockPacket, 1);
  else
    host_.capabilityHost()->prep(_id, name(), s, GetBlockPacket, 1);
  s.append(hash);
  auto peer = getPeer(_id);
  if (peer) peer->setAsking(true);
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestPbftBlocks(NodeID const &_id,
                                         size_t height_to_sync) {
  RLPStream s;
  std::vector<uint8_t> bytes;
  host_.capabilityHost()->prep(_id, name(), s, GetPbftBlockPacket, 1);
  s << height_to_sync;
  LOG(log_dg_) << "Sending GetPbftBlockPacket with size: " << height_to_sync;
  auto peer = getPeer(_id);
  if (peer) peer->setAsking(true);
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlocksLevel(NodeID const &_id,
                                          unsigned long level,
                                          int number_of_levels) {
  RLPStream s;
  std::vector<uint8_t> bytes;
  host_.capabilityHost()->prep(_id, name(), s, GetBlocksLevelPacket, 2);
  s << level;
  s << number_of_levels;
  LOG(log_nf_) << "Sending GetBlocksLevelPacket of level:" << level << " "
               << number_of_levels;
  auto peer = getPeer(_id);
  if (peer) peer->setAsking(true);
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

void TaraxaCapability::doBackgroundWork() {
  if (auto full_node = full_node_.lock()) {
    onNewTransactions(full_node->getNewVerifiedTrxSnapShotSerialized(), false);
  }
  for (auto const &peer : peers_) {
    time_t now =
        std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    // Disconnect any node that do not respond within 10 seconds
    if (peer.second->asking() && now - peer.second->lastAsk() > 10) {
      LOG(log_nf_) << "Host disconnected, no response in 10 seconds"
                   << peer.first;
      host_.capabilityHost()->disconnect(peer.first, p2p::PingTimeout);
    }
    // Disconnect any node that did not send any message for over 120
    // seconds
    if (now - peer.second->lastMessageTime() > 120) {
      LOG(log_nf_) << "Host disconnected, no message in 120 seconds"
                   << peer.first;
      host_.capabilityHost()->disconnect(peer.first, p2p::PingTimeout);
    }
  }
  host_.scheduleExecution(conf_.network_transaction_interval,
                          [this]() { doBackgroundWork(); });
}

void TaraxaCapability::onStarting() {
  if (conf_.network_simulated_delay > 0) {
    const int number_of_delayed_threads = 5;
    io_work_ = std::make_shared<boost::asio::io_service::work>(io_service_);
    for (int i = 0; i < number_of_delayed_threads; ++i)
      delay_threads_.create_thread([&]() { io_service_.run(); });
  }
  if (conf_.network_transaction_interval > 0)
    host_.scheduleExecution(conf_.network_transaction_interval,
                            [this]() { doBackgroundWork(); });
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
  LOG(log_dg_) << "sendPbftVote " << vote.getHash() << " to " << _id;

  RLPStream s;
  std::vector<uint8_t> bytes;

  // Need to put a scope of vectorstream, other bytes won't get result.
  {
    vectorstream strm(bytes);
    vote.serialize(strm);
  }
  host_.capabilityHost()->prep(_id, name(), s, PbftVotePacket, 1);
  s.appendList(bytes.size());
  for (auto i = 0; i < bytes.size(); i++) {
    s << bytes[i];
  }
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::onNewPbftBlock(taraxa::PbftBlock const &pbft_block) {
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);
  for (auto const &peer : peers_) {
    if (!peer.second->isPbftBlockKnown(pbft_block.getBlockHash())) {
      sendPbftBlock(peer.first, pbft_block);
    }
  }
}

// api for pbft syncing
void TaraxaCapability::sendPbftBlocks(NodeID const &_id, size_t height_to_sync,
                                      size_t blocks_to_transfer) {
  LOG(log_dg_) << "In sendPbftBlocks, peer want to sync from pbft chain height "
               << height_to_sync << ", will send " << blocks_to_transfer
               << " pbft blocks to " << _id;
  if (auto full_node = full_node_.lock()) {
    auto pbftchain = full_node->getPbftChain();
    assert(pbftchain);
    auto vote_db = full_node->getVotesDB();
    assert(vote_db);
    // add cert votes for each pbftblock
    std::vector<PbftBlock> blocks =
        pbftchain->getPbftBlocks(height_to_sync, blocks_to_transfer);
    std::vector<PbftBlockCert> cert_blocks;
    // has some redundancy here. fix later
    for (auto const &b : blocks) {
      auto cert_votes_rlp = vote_db->lookup(b.getBlockHash());
      PbftBlockCert bk(b, cert_votes_rlp);
      cert_blocks.emplace_back(bk);
    }

    // Protect PBFT chain DB processing happening in the same time
    if (cert_blocks.empty()) {
      return;
    }
    RLPStream s;
    host_.capabilityHost()->prep(_id, name(), s, PbftBlockPacket,
                                 cert_blocks.size());
    for (auto const &b : cert_blocks) s.append(b.rlp());
    host_.capabilityHost()->sealAndSend(_id, s);
    // Question: will send multiple times to a same receiver, why?
    LOG(log_dg_) << "Sending PbftCertBlocks " << cert_blocks << " to " << _id;
  }
}

void TaraxaCapability::sendPbftBlock(NodeID const &_id,
                                     taraxa::PbftBlock const &pbft_block) {
  LOG(log_dg_) << "sendPbftBlock " << pbft_block.getBlockHash() << " to "
               << _id;

  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, NewPbftBlockPacket, 1);
  pbft_block.serializeRLP(s);
  host_.capabilityHost()->sealAndSend(_id, s);
}

std::string TaraxaCapability::packetToPacketName(byte const &packet) {
  switch (packet) {
    case StatusPacket:
      return "StatusPacket";
    case NewBlockPacket:
      return "NewBlockPacket";
    case NewBlockHashPacket:
      return "NewBlockHashPacket";
    case GetNewBlockPacket:
      return "GetNewBlockPacket";
    case GetBlockPacket:
      return "GetBlockPacket";
    case BlockPacket:
      return "BlockPacket";
    case GetBlocksLevelPacket:
      return "GetBlocksLevelPacket";
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