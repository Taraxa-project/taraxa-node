#include "taraxa_capability.h"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "vote.h"

using namespace taraxa;

void TaraxaCapability::syncPeer(NodeID const &_nodeID) {
  if (auto full_node = full_node_.lock()) {
    LOG(logger_) << "Sync Peer:" << _nodeID.toString();
    peers_[_nodeID].m_state = Syncing;
    auto leaves = full_node->collectTotalLeaves();
    requestBlockChildren(_nodeID, leaves);
  }
}

void TaraxaCapability::continueSync(NodeID const &_nodeID) {
  if (auto full_node = full_node_.lock()) {
    for (auto block : peers_[_nodeID].m_syncBlocks) {
      for (auto tip : block.second.first.getTips()) {
        auto tipKnown = full_node->isBlockKnown(tip);
        if (!tipKnown && peers_[_nodeID].m_syncBlocks.find(tip) ==
                             peers_[_nodeID].m_syncBlocks.end()) {
          peers_[_nodeID].m_lastRequest = tip;
          LOG(logger_) << "Block " << block.second.first.getHash().toString()
                       << " has a missing tip " << tip.toString();
          requestBlock(_nodeID, tip, false);
          return;
        }
      }
    }
    for (auto block : peers_[_nodeID].m_syncBlocks) {
      if (!full_node->isBlockKnown(block.first)) {
        LOG(logger_) << "Storing block "
                     << block.second.first.getHash().toString() << " with " << block.second.second.size() << " transactions";
        full_node->storeBlockWithTransactions(block.second.first,
                                              block.second.second);
      }
    }
    // After storing blocks, we need to give some time for the blocks to be
    // processed before continuing sync
    // Better solution needed later
    bool blockInvalid = false;
    for (auto block : peers_[_nodeID].m_syncBlocks) {
      for (int i = 0; i < 100; i++) {
        if (full_node->getDagBlockFromDb(block.first) != nullptr) break;
        if(i == 9) {
          blockInvalid = true;
          break;
        }
        thisThreadSleepForMilliSeconds(100);
      }
    }
    peers_[_nodeID].m_syncBlocks.clear();
    if(blockInvalid) return;//This would probably mean that the peer is corrupted as well
    

    if (peers_[_nodeID].m_state == Syncing) syncPeer(_nodeID);
  }
}

void TaraxaCapability::onConnect(NodeID const &_nodeID, u256 const &) {
  LOG(logger_) << "Node " << _nodeID << " connected";
  cnt_received_messages_[_nodeID] = 0;
  test_sums_[_nodeID] = 0;

  TaraxaPeer peer(_nodeID);
  peers_.emplace(_nodeID, peer);
  syncPeer(_nodeID);
}

bool TaraxaCapability::interpretCapabilityPacket(NodeID const &_nodeID,
                                                 unsigned _id, RLP const &_r) {
  if (network_simulated_delay_ == 0) {
    return interpretCapabilityPacketImpl(_nodeID, _id, _r);
  }
  // RLP contains memory it does not own so deep copy of bytes is needed
  dev::bytes rBytes = _r.data().toBytes();
  int messageSize = rBytes.size() * 8;
  unsigned int dist = *((int *)this->host_.id().data()) ^ *((int *)_nodeID.data());
  unsigned int delay = dist % network_simulated_delay_;
  //Use next 16 bits for bandwidth value
  dist = dist >> 16;
  //Bandwidth is for now hard coded in random range between 1Mb/s and 1000Mb/s
  unsigned int bandwidth_delay = messageSize / (dist % 1000 + 1) / 1000;
  //Random component up to +-10% 
  int random_component = random_dist_(delay_rng_);
  unsigned int total_delay = (delay + bandwidth_delay) * random_component / 100;
  LOG(logger_debug_) << "Delaying packet by: " << delay << " milliseconds";
  auto timer = std::make_shared<boost::asio::deadline_timer>(io_service_);
  timer->expires_from_now(boost::posix_time::milliseconds(total_delay));
  timer->async_wait(([this, _nodeID, _id, rBytes, timer](const boost::system::error_code& ec) {
    RLP _rCopy(rBytes);
    interpretCapabilityPacketImpl(_nodeID, _id, _rCopy);
  }));
  return true;
}

bool TaraxaCapability::interpretCapabilityPacketImpl(NodeID const &_nodeID,
                                                     unsigned _id,
                                                     RLP const &_r) {
  switch (_id) {
    case NewBlockPacket: {
      LOG(logger_debug_) << "Received NewBlockPacket";
      DagBlock block(_r[0]);

      auto transactionsCount = _r.itemCount() - 1;
      std::vector<Transaction> newTransactions;
      for (auto iTransaction = 1; iTransaction < transactionsCount + 1;
           iTransaction++) {
        Transaction transaction(_r[iTransaction]);
        newTransactions.push_back(transaction);
        peers_[_nodeID].markTransactionAsKnown(transaction.getHash());
      }

      peers_[_nodeID].markBlockAsKnown(block.getHash());
      onNewBlockReceived(block, newTransactions);
      break;
    }
    case BlockPacket: {
      DagBlock block(_r[0]);

      auto transactionsCount = _r.itemCount() - 1;
      std::unordered_map<trx_hash_t, Transaction> newTransactions;
      for (auto iTransaction = 1; iTransaction < transactionsCount + 1;
           iTransaction++) {
        Transaction transaction(_r[iTransaction]);
        newTransactions[transaction.getHash()] = transaction;
        peers_[_nodeID].markTransactionAsKnown(transaction.getHash());
      }

      LOG(logger_debug_) << "Received BlockPacket "
                         << block.getHash().toString();
      peers_[_nodeID].markBlockAsKnown(block.getHash());
      if (peers_[_nodeID].m_lastRequest == block.getHash()) {
        std::vector<Transaction> vTransactions;
        for (const auto &t : newTransactions) vTransactions.push_back(t.second);
        peers_[_nodeID].m_syncBlocks[block.getHash()] = {block, vTransactions};
        continueSync(_nodeID);
      } else if (auto full_node = full_node_.lock()) {
        LOG(logger_) << "Storing " << newTransactions.size() << " transactions";
        full_node->insertNewTransactions(newTransactions);
        LOG(logger_) << "Storing block "
                     << block.getHash().toString();
        full_node->storeBlock(block);
      } else {
        for (const auto &transaction : newTransactions) {
          if (test_transactions_.find(transaction.first) ==
              test_transactions_.end()) {
            test_transactions_[transaction.first] = transaction.second;
            LOG(logger_debug_)
                << "Received New Transaction " << transaction.first.toString();
          } else {
            LOG(logger_debug_)
                << "Received New Transaction" << transaction.first.toString()
                << "that is already known";
          }
        }
        test_blocks_[block.getHash()] = block;
      }
    } break;
    case NewBlockHashPacket: {
      blk_hash_t hash(_r[0]);
      LOG(logger_debug_) << "Received NewBlockHashPacket" << hash.toString();
      peers_[_nodeID].markBlockAsKnown(hash);
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
      LOG(logger_debug_) << "Received GetBlockPacket" << hash.toString();
      peers_[_nodeID].markBlockAsKnown(hash);
      if (auto full_node = full_node_.lock()) {
        auto block = full_node->getDagBlock(hash);
        if (block) {
          sendBlock(_nodeID, *block, false);
        } else
          LOG(logger_) << "NO PACKET: " << hash.toString();
      }
      break;
    }
    case GetNewBlockPacket: {
      blk_hash_t hash(_r[0]);
      peers_[_nodeID].markBlockAsKnown(hash);
      LOG(logger_debug_) << "Received GetNewBlockPacket" << hash.toString();

      if (auto full_node = full_node_.lock()) {
        auto block = full_node->getDagBlock(hash);
        if (block) {
          sendBlock(_nodeID, *block, true);
        } else
          LOG(logger_) << "NO NEW PACKET: " << hash.toString();
      } else if (test_blocks_.find(hash) != test_blocks_.end()) {
        sendBlock(_nodeID, test_blocks_[hash], true);
      }
      break;
    }
    case GetBlockChildrenPacket: {
      LOG(logger_debug_) << "Received GetBlockChildrenPacket with "
                         << _r.itemCount() << " child blocks";
      auto blockCount = _r.itemCount();
      dev::strings totalChildren;
      for (auto iBlock = 0; iBlock < blockCount; iBlock++) {
        blk_hash_t hash(_r[iBlock]);
        if (auto full_node = full_node_.lock()) {
          auto children = full_node->getTotalDagBlockChildren(hash, ULONG_MAX);
          LOG(logger_debug_) << "Found " << children.size() << " children";
          totalChildren += children;
        }
      }
      sendChildren(_nodeID, totalChildren);
      break;
    }
    case BlockChildrenPacket: {
      std::string receivedBlocks;
      auto itemCount = _r.itemCount();

      int transactionCount = 0;
      for (auto iBlock = 0; iBlock < itemCount; iBlock++) {
        DagBlock block(_r[iBlock + transactionCount]);
        peers_[_nodeID].markBlockAsKnown(block.getHash());

        std::vector<Transaction> newTransactions;
        for (int i = 0; i < block.getTrxs().size(); i++) {
          transactionCount++;
          Transaction transaction(_r[iBlock + transactionCount]);
          newTransactions.push_back(transaction);
          peers_[_nodeID].markTransactionAsKnown(transaction.getHash());
        }

        receivedBlocks += block.getHash().toString() + " ";
        peers_[_nodeID].m_syncBlocks[block.getHash()] = {block,
                                                          newTransactions};
        if (iBlock + transactionCount + 1 >= itemCount) break;
      }
      if (itemCount > 0) {
        LOG(logger_debug_) << "Received BlockChildrenPacket with "
                           << _r.itemCount()
                           << " child blocks:" << receivedBlocks.c_str();
        continueSync(_nodeID);
      }
      break;
    }
    case TransactionPacket: {
      std::string receivedTransactions;
      std::unordered_map<trx_hash_t, Transaction> transactions;
      auto transactionCount = _r.itemCount();
      for (auto iTransaction = 0; iTransaction < transactionCount;
           iTransaction++) {
        Transaction transaction(_r[iTransaction]);
        receivedTransactions += transaction.getHash().toString() + " ";
        peers_[_nodeID].markTransactionAsKnown(transaction.getHash());
        transactions[transaction.getHash()] = transaction;
      }
      if (transactionCount > 0) {
        LOG(logger_debug_) << "Received TransactionPacket with "
                           << _r.itemCount()
                           << " transactions:" << receivedTransactions.c_str();
        onNewTransactions(transactions, true);
      }
      break;
    }
    case PbftVotePacket: {
      LOG(logger_debug_) << "Received PBFT vote";

      std::vector<::byte> pbft_vote_bytes;

      for (auto i = 0; i < _r[0].itemCount(); i++) {
        pbft_vote_bytes.push_back(_r[0][i].toInt());
      }
      taraxa::bufferstream strm(pbft_vote_bytes.data(), pbft_vote_bytes.size());
      Vote vote;
      vote.deserialize(strm);

      peers_[_nodeID].markVoteAsKnown(vote.getHash());

      auto full_node = full_node_.lock();
      if (!full_node) {
        LOG(logger_err_) << "PbftVote full node weak pointer empty";
        return false;
      }

      if (!full_node->isKnownVote(vote)) {
        full_node->placeVote(vote);
        full_node->setVoteKnown(vote);
        onNewPbftVote(vote);
      }

      break;
    }
    case PbftBlockPacket: {
      LOG(logger_debug_) << "Received PBFT Block";

      PbftBlock pbft_block(_r[0]);
      peers_[_nodeID].markPbftBlockAsKnown(pbft_block.getBlockHash());

      auto full_node = full_node_.lock();
      if (!full_node) {
        LOG(logger_err_) << "PbftBlock full node weak pointer empty";
        return false;
      }
      if (!full_node->isKnownPbftBlock(pbft_block.getBlockHash())) {
        full_node->pushPbftBlockIntoQueue(pbft_block);
        onNewPbftBlock(pbft_block);
      }

      break;
    }
    case TestPacket:
      LOG(logger_debug_) << "Received TestPacket";
      ++cnt_received_messages_[_nodeID];
      test_sums_[_nodeID] += _r[0].toInt();
      BOOST_ASSERT(_id == TestPacket);
      return (_id == TestPacket);
  };
  return true;
}
void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  LOG(logger_) << "Node " << _nodeID << " disconnected";
  cnt_received_messages_.erase(_nodeID);
  test_sums_.erase(_nodeID);
}

void TaraxaCapability::sendTestMessage(NodeID const &_id, int _x) {
  RLPStream s;
  host_.capabilityHost()->sealAndSend(
      _id, host_.capabilityHost()->prep(_id, name(), s, TestPacket, 1) << _x);
}

vector<NodeID> TaraxaCapability::selectPeers(
    std::function<bool(TaraxaPeer const &)> const &_predicate) {
  vector<NodeID> allowed;
  for (auto const &peer : peers_) {
    if (_predicate(peer.second)) allowed.push_back(peer.first);
  }
  return allowed;
}

vector<NodeID> TaraxaCapability::getAllPeers() const {
  vector<NodeID> peers;
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
    std::unordered_map<trx_hash_t, Transaction> const &transactions,
    bool fromNetwork) {
  if (fromNetwork) {
    if (auto full_node = full_node_.lock()) {
      LOG(logger_) << "Storing " << transactions.size() << " transactions";
        full_node->insertNewTransactions(transactions);
    } else {
      for (auto const &transaction : transactions) {
        if (test_transactions_.find(transaction.first) ==
            test_transactions_.end()) {
          test_transactions_[transaction.first] = transaction.second;
          LOG(logger_debug_)
              << "Received New Transaction " << transaction.first.toString();
        } else {
          LOG(logger_debug_)
              << "Received New Transaction" << transaction.first.toString()
              << "that is already known";
        }
      }
    }
  } else {
    for (auto &peer : peers_) {
      std::vector<Transaction> transactionsToSend;
      for (auto const &transaction : transactions) {
        if (!peer.second.isTransactionKnown(transaction.first)) {
          peer.second.markTransactionAsKnown(transaction.first);
          transactionsToSend.push_back(transaction.second);
        }
      }
      if (transactionsToSend.size() > 0)
        sendTransactions(peer.first, transactionsToSend);
    }
  }
}

void TaraxaCapability::onNewBlockReceived(
    DagBlock block, std::vector<Transaction> transactions) {
  if (auto full_node = full_node_.lock()) {
    if (full_node->isBlockKnown(block.getHash())) {
      LOG(logger_debug_) << "Received NewBlock " << block.getHash().toString()
                         << "that is already known";
      return;
    } else {
      LOG(logger_) << "Storing block "
                     << block.getHash().toString() << " with " << transactions.size() << " transactions";
      full_node->storeBlockWithTransactions(block, transactions);
    }
  } else if (test_blocks_.find(block.getHash()) == test_blocks_.end()) {
    test_blocks_[block.getHash()] = block;
    for (auto tr : transactions) {
      test_transactions_[tr.getHash()] = tr;
    }
    onNewBlockVerified(block);

  } else {
    LOG(logger_debug_) << "Received NewBlock " << block.getHash().toString()
                       << "that is already known";
    return;
  }
}

void TaraxaCapability::onNewBlockVerified(DagBlock block) {
  LOG(logger_debug_) << "Verified NewBlock " << block.getHash().toString();
  const int c_minBlockBroadcastPeers = 10;
  auto const peersWithoutBlock = selectPeers([&](TaraxaPeer const &_peer) {
    return !_peer.isBlockKnown(block.getHash());
  });

  auto const peersToSendNumber = std::max<std::size_t>(
      c_minBlockBroadcastPeers, std::sqrt(peers_.size()));

  std::vector<NodeID> peersToSend;
  std::vector<NodeID> peersToAnnounce;
  std::tie(peersToSend, peersToAnnounce) =
      randomPartitionPeers(peersWithoutBlock, peersToSendNumber);

  for (NodeID const &peerID : peersToSend) {
    RLPStream ts;
    auto itPeer = peers_.find(peerID);
    if (itPeer != peers_.end()) {
      sendBlock(peerID, block, true);
      itPeer->second.markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToSend.empty())
    LOG(logger_debug_) << "Sent block to" << peersToSend.size() << " peers";

  for (NodeID const &peerID : peersToAnnounce) {
    RLPStream ts;
    auto itPeer = peers_.find(peerID);
    if (itPeer != peers_.end()) {
      sendBlockHash(peerID, block);
      itPeer->second.markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToAnnounce.empty())
    LOG(logger_debug_) << "Anounced block to " << peersToAnnounce.size()
                       << " peers";
}

void TaraxaCapability::sendChildren(NodeID const &_id,
                                    std::vector<std::string> children) {
  LOG(logger_debug_) << "sendChildren " << children.size();
  RLPStream s;
  std::vector<DagBlock> blocksToSend;
  std::vector<std::vector<Transaction>> blockTransactions;
  int totalTransactionsCount = 0;
  if (auto full_node = full_node_.lock()) {
    for (auto child : children) {
      auto block = full_node->getDagBlock(blk_hash_t(child));
      std::vector<Transaction> transactions;
      for (auto trx : block->getTrxs()) {
        transactions.push_back(*full_node->getTransaction(trx));
        totalTransactionsCount++;
      }
      blocksToSend.push_back(*block);
      blockTransactions.push_back(transactions);
    }
  }
  host_.capabilityHost()->prep(_id, name(), s, BlockChildrenPacket,
                                children.size() + totalTransactionsCount);
  for (int iBlock = 0; iBlock < blocksToSend.size(); iBlock++) {
    blocksToSend[iBlock].serializeRLP(s);
    for (auto &trx : blockTransactions[iBlock]) {
      trx.serializeRLP(s);
    }
  }
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendTransactions(
    NodeID const &_id, std::vector<Transaction> const &transactions) {
  LOG(logger_debug_) << "sendTransactions" << transactions.size() << " to "
                     << _id;
  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, TransactionPacket,
                                transactions.size());
  for (auto transaction : transactions) {
    transaction.serializeRLP(s);
  }
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendBlock(NodeID const &_id, taraxa::DagBlock block,
                                 bool newBlock) {
  LOG(logger_debug_) << "sendBlock " << block.getHash().toString();
  RLPStream s;
  vec_trx_t transactionsToSend;
  if (newBlock) {
    for (auto trx : block.getTrxs()) {
      if (!peers_[_id].isTransactionKnown(trx))
        transactionsToSend.push_back(trx);
    }
    host_.capabilityHost()->prep(_id, name(), s, NewBlockPacket,
                                  1 + transactionsToSend.size());
  } else {
    host_.capabilityHost()->prep(_id, name(), s, BlockPacket,
                                  1 + block.getTrxs().size());
    transactionsToSend = block.getTrxs();
  }
  block.serializeRLP(s);

  for (auto trx : transactionsToSend) {
    std::shared_ptr<Transaction> transaction;
    if (auto full_node = full_node_.lock()) {
      transaction = full_node->getTransaction(trx);
    } else {
      assert(test_transactions_.find(trx) != test_transactions_.end());
      transaction = std::make_shared<Transaction>(test_transactions_[trx]);
    }
    assert(transaction != nullptr);  // We should never try to send a block for
                                     // which  we do not have all transactions
    transaction->serializeRLP(s);
  }
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendBlockHash(NodeID const &_id,
                                     taraxa::DagBlock block) {
  LOG(logger_debug_) << "sendBlockHash " << block.getHash().toString();
  RLPStream s;
  std::vector<uint8_t> bytes;
  host_.capabilityHost()->prep(_id, name(), s, NewBlockHashPacket, 1);
  s.append(block.getHash());
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlock(NodeID const &_id, blk_hash_t hash,
                                    bool newBlock) {
  LOG(logger_debug_) << "requestBlock " << hash.toString();
  RLPStream s;
  std::vector<uint8_t> bytes;
  if (newBlock)
    host_.capabilityHost()->prep(_id, name(), s, GetNewBlockPacket, 1);
  else
    host_.capabilityHost()->prep(_id, name(), s, GetBlockPacket, 1);
  s.append(hash);
  host_.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlockChildren(NodeID const &_id,
                                            std::vector<std::string> leaves) {
  RLPStream s;
  std::vector<uint8_t> bytes;
  host_.capabilityHost()->prep(_id, name(), s, GetBlockChildrenPacket,
                                leaves.size());
  std::string blocks;
  for (auto leaf : leaves) {
    blocks += leaf + " ";
    blk_hash_t bHash(leaf);
    s.append(bHash);
  }
  LOG(logger_debug_) << "Sending GetBlockChildrenPacket of blocks:" << blocks;
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
    onNewTransactions(full_node->getNewVerifiedTrxSnapShot(true), false);
  }
  host_.scheduleExecution(c_backround_work_period_ms_,
                           [this]() { doBackgroundWork(); });
}

void TaraxaCapability::onStarting() {
  if (network_simulated_delay_ > 0) {
      const int number_of_delayed_threads = 5;
      io_work_ = std::make_shared<boost::asio::io_service::work>(io_service_);
      for (int i = 0; i < number_of_delayed_threads; ++i)
        delay_threads_.create_thread([&]() {
          io_service_.run();
        });
    }
  host_.scheduleExecution(c_backround_work_period_ms_,
                           [this]() { doBackgroundWork(); });
}

void TaraxaCapability::onNewPbftVote(taraxa::Vote const &vote) {
  for (auto const& peer : peers_) {
    if (!peer.second.isVoteKnown(vote.getHash())) {
      sendPbftVote(peer.first, vote);
    }
  }
}

void TaraxaCapability::sendPbftVote(NodeID const &_id,
                                    taraxa::Vote const &vote) {
  LOG(logger_debug_) << "sendPbftVote " << vote.getHash() << " to " << _id;

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
  for (auto const& peer: peers_) {
    if (!peer.second.isPbftBlockKnown(pbft_block.getBlockHash())) {
      sendPbftBlock(peer.first, pbft_block);
    }
  }
}

void TaraxaCapability::sendPbftBlock(NodeID const &_id,
                                     taraxa::PbftBlock const &pbft_block) {
  LOG(logger_debug_) << "sendPbftBlock " << pbft_block.getBlockHash()
                     << " to " << _id;

  RLPStream s;
  host_.capabilityHost()->prep(_id, name(), s, PbftBlockPacket, 1);
  pbft_block.serializeRLP(s);
  host_.capabilityHost()->sealAndSend(_id, s);
}
