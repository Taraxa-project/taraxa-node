#include "taraxa_capability.h"

using namespace taraxa;

string shortHash(const string &str) {
  size_t first = str.find_first_not_of('0');
  if (string::npos == first) {
    return "GENESIS";
  }
  return str.substr(first);
}

void TaraxaCapability::syncPeer(NodeID const &_nodeID) {
  if (full_node_) {
    printf("Sync Peer\n");
    m_peers[_nodeID].m_state = Syncing;
    blk_hash_t pivot = full_node_->getLatestPivot();
    printf("Pivot: %s\n", shortHash(pivot.toString()).c_str());
    // if(pivot == blk_hash_t())
    //	pivot == Dag::GENESIS; Not neededs since GENEIS is all
    // 00000
    requestBlockChildren(_nodeID, pivot);
  }
}

void TaraxaCapability::continueSync(NodeID const &_nodeID) {
  for (auto block : m_peers[_nodeID].m_syncBlocks) {
    for (auto tip : block.second.getTips()) {
      auto tipBlock = full_node_->getDagBlock(tip);
      if (!tipBlock && m_peers[_nodeID].m_syncBlocks.find(tip) ==
                           m_peers[_nodeID].m_syncBlocks.end()) {
        m_peers[_nodeID].m_lastRequest = tip;
        printf("Block %s has a missing tip %s\n",
               shortHash(block.second.getHash().toString()).c_str(),
               shortHash(tip.toString()).c_str());
        requestBlock(_nodeID, tip, false);
        return;
      }
    }
  }
  for (auto block : m_peers[_nodeID].m_syncBlocks) {
    printf("Storing block %s\n",
           shortHash(block.second.getHash().toString()).c_str());
    full_node_->storeBlock(block.second);
  }
  m_peers[_nodeID].m_syncBlocks.clear();
  if (m_peers[_nodeID].m_state == Syncing) syncPeer(_nodeID);
}

void TaraxaCapability::onConnect(NodeID const &_nodeID, u256 const &) {
  m_cntReceivedMessages[_nodeID] = 0;
  m_testSums[_nodeID] = 0;

  TaraxaPeer peer(_nodeID);
  m_peers.emplace(_nodeID, peer);

  blk_hash_t pivot = full_node_->getLatestPivot();
  if (shortHash(pivot.toString()) == "GENESIS") syncPeer(_nodeID);
}
bool TaraxaCapability::interpretCapabilityPacket(NodeID const &_nodeID,
                                                 unsigned _id, RLP const &_r) {
  switch (_id) {
    case NewBlockPacket: {
      printf("Received NewBlockPacket\n");
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
      DagBlock block;
      block.deserialize(strm);

      m_peers[_nodeID].markBlockAsKnown(block.getHash());
      if (m_blocks.find(block.getHash()) == m_blocks.end()) {
        onNewBlock(block);
        printf("Received NewBlock\n");
        if (full_node_) {
          BlockVisitor visitor(full_node_);
          taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
          visitor.visit(strm);
        }
      }
      break;
    }
    case BlockPacket: {
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
      DagBlock block;
      block.deserialize(strm);

      printf("Received BlockPacket %s\n",
             shortHash(block.getHash().toString()).c_str());
      m_peers[_nodeID].markBlockAsKnown(block.getHash());
      if (m_blocks.find(block.getHash()) == m_blocks.end()) {
        if (m_peers[_nodeID].m_lastRequest == block.getHash()) {
          m_peers[_nodeID].m_syncBlocks[block.getHash()] = block;
          continueSync(_nodeID);
        } else if (full_node_ && full_node_.lock()) {
          BlockVisitor visitor(full_node_);
          taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
          visitor.visit(strm);
        }
      }
      break;
    }
    case NewBlockHashPacket: {
      printf("Received NewBlockHashPacket\n");
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      blk_hash_t hash;
      memcpy(&hash, blockBytes.data(), blockBytes.size());
      if (m_blocks.find(hash) == m_blocks.end() &&
          m_blockRequestedSet.count(hash) == 0) {
        m_blockRequestedSet.insert(hash);
        requestBlock(_nodeID, hash, true);
      }
      break;
    }
    case GetBlockPacket: {
      printf("Received GetBlockPacket\n");
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      blk_hash_t hash;
      memcpy(&hash, blockBytes.data(), blockBytes.size());
      auto block = full_node_->getDagBlock(hash);
      if (block) {
        sendBlock(_nodeID, *block, false);
      }
      break;
    }
    case GetNewBlockPacket: {
      printf("Received GetBlockPacket\n");
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      blk_hash_t hash;
      memcpy(&hash, blockBytes.data(), blockBytes.size());
      if (m_blocks.find(hash) != m_blocks.end()) {
        sendBlock(_nodeID, m_blocks[hash], true);
      }
      break;
    }
    case GetBlockChildrenPacket: {
      printf("Received GetBlockChildrenPacket\n");
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      blk_hash_t hash;
      memcpy(&hash, blockBytes.data(), blockBytes.size());
      if (full_node_) {
        auto children = full_node_->getDagBlockChildren(hash, ULONG_MAX);
        printf("Found %lu children\n", children.size());
        sendChildren(_nodeID, children);
      }
      break;
    }
    case BlockChildrenPacket: {
      printf("Received BlockChildrenPacket with %lu child blocks:",
             _r.itemCount());
      auto blockCount = _r.itemCount();
      for (auto iBlock = 0; iBlock < blockCount; iBlock++) {
        std::vector<::byte> blockBytes;
        for (auto i = 0; i < _r[iBlock].itemCount(); i++) {
          blockBytes.push_back(_r[iBlock][i].toInt());
        }
        taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
        DagBlock block;
        block.deserialize(strm);
        printf("%s ", shortHash(block.getHash().toString()).c_str());
        m_peers[_nodeID].m_syncBlocks[block.getHash()] = block;
      }
      if (blockCount > 0) {
        continueSync(_nodeID);
        printf("\n");
      } else
        requestLeaves(_nodeID);
      break;
    }
    case GetLeavesPacket: {
      printf("Received GetLeavesPacket\n");
      std::vector<::byte> blockBytes;
      for (auto i = 0; i < _r[0].itemCount(); i++) {
        blockBytes.push_back(_r[0][i].toInt());
      }
      blk_hash_t hash;
      memcpy(&hash, blockBytes.data(), blockBytes.size());
      if (full_node_) {
        auto leaves = full_node_->collectLeaves();
        printf("Found %lu leaves\n", leaves.size());
        sendLeaves(_nodeID, leaves);
      }
      break;
    }
    case LeavesPacket: {
      printf("Received LeavesPacket %lu %lu\n", _r.size(), _r.itemCount());
      auto blockCount = _r.itemCount();
      for (auto iBlock = 0; iBlock < blockCount; iBlock++) {
        std::vector<::byte> blockBytes;
        for (auto i = 0; i < _r[iBlock].itemCount(); i++) {
          blockBytes.push_back(_r[iBlock][i].toInt());
        }
        taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
        DagBlock block;
        block.deserialize(strm);
        // printf("empl %s\n", shortHash(block.getHash().toString()).c_str());
        m_peers[_nodeID].m_syncBlocks[block.getHash()] = block;
      }
      if (blockCount > 0) continueSync(_nodeID);
      break;
    }
    case TestPacket:
      printf("Received TestPacket\n");
      ++m_cntReceivedMessages[_nodeID];
      m_testSums[_nodeID] += _r[0].toInt();
      BOOST_ASSERT(_id == TestPacket);
      return (_id == TestPacket);
  };
  return true;
}
void TaraxaCapability::onDisconnect(NodeID const &_nodeID) {
  m_cntReceivedMessages.erase(_nodeID);
  m_testSums.erase(_nodeID);
}

void TaraxaCapability::sendTestMessage(NodeID const &_id, int _x) {
  RLPStream s;
  m_host.capabilityHost()->sealAndSend(
      _id, m_host.capabilityHost()->prep(_id, name(), s, TestPacket, 1) << _x);
}

vector<NodeID> TaraxaCapability::selectPeers(
    std::function<bool(TaraxaPeer const &)> const &_predicate) {
  vector<NodeID> allowed;
  for (auto const &peer : m_peers) {
    if (_predicate(peer.second)) allowed.push_back(peer.first);
  }
  return allowed;
}

std::pair<std::vector<NodeID>, std::vector<NodeID>>
TaraxaCapability::randomPartitionPeers(std::vector<NodeID> const &_peers,
                                       std::size_t _number) {
  vector<NodeID> part1(_peers);
  vector<NodeID> part2;

  if (_number >= _peers.size()) return std::make_pair(part1, part2);

  std::shuffle(part1.begin(), part1.end(), m_urng);

  // Remove elements from the end of the shuffled part1 vector and move
  // them to part2.
  std::move(part1.begin() + _number, part1.end(), std::back_inserter(part2));
  part1.erase(part1.begin() + _number, part1.end());
  return std::make_pair(move(part1), move(part2));
}

void TaraxaCapability::onNewBlock(DagBlock block) {
  const int c_minBlockBroadcastPeers = 1;
  m_blocks[block.getHash()] = block;
  auto const peersWithoutBlock = selectPeers([&](TaraxaPeer const &_peer) {
    return !_peer.isBlockKnown(block.getHash());
  });

  auto const peersToSendNumber = std::max<std::size_t>(
      c_minBlockBroadcastPeers, std::sqrt(m_peers.size()));

  std::vector<NodeID> peersToSend;
  std::vector<NodeID> peersToAnnounce;
  std::tie(peersToSend, peersToAnnounce) =
      randomPartitionPeers(peersWithoutBlock, peersToSendNumber);

  for (NodeID const &peerID : peersToSend) {
    RLPStream ts;
    auto itPeer = m_peers.find(peerID);
    if (itPeer != m_peers.end()) {
      sendBlock(peerID, block, true);
      itPeer->second.markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToSend.empty())
    printf("Sent block to %lu peers\n", peersToSend.size());

  for (NodeID const &peerID : peersToAnnounce) {
    RLPStream ts;
    auto itPeer = m_peers.find(peerID);
    if (itPeer != m_peers.end()) {
      sendBlockHash(peerID, block);
      itPeer->second.markBlockAsKnown(block.getHash());
    }
  }
  if (!peersToAnnounce.empty())
    printf("Anounced block to %lu peers\n", peersToAnnounce.size());
}

void TaraxaCapability::sendChildren(NodeID const &_id,
                                    std::vector<std::string> children) {
  // printf("sendBlock \n");
  RLPStream s;
  m_host.capabilityHost()->prep(_id, name(), s, BlockChildrenPacket,
                                children.size());
  for (auto child : children) {
    auto block = full_node_->getDagBlock(child);
    std::vector<uint8_t> bytes;
    // Need to put a scope of vectorstream, other bytes won't get
    // result.
    {
      vectorstream strm(bytes);
      block->serialize(strm);
    }
    s.appendList(bytes.size());
    for (auto i = 0; i < bytes.size(); i++) s << bytes[i];
  }
  m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendLeaves(NodeID const &_id,
                                  std::vector<std::string> leaves) {
  // printf("sendBlock \n");
  RLPStream s;
  m_host.capabilityHost()->prep(_id, name(), s, LeavesPacket, leaves.size());
  for (auto leave : leaves) {
    auto block = full_node_->getDagBlock(leave);
    std::vector<uint8_t> bytes;
    // Need to put a scope of vectorstream, other bytes won't get
    // result.
    {
      vectorstream strm(bytes);
      block->serialize(strm);
    }
    s.appendList(bytes.size());
    for (auto i = 0; i < bytes.size(); i++) s << bytes[i];
  }
  m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendBlock(NodeID const &_id, taraxa::DagBlock block,
                                 bool newBlock) {
  // printf("sendBlock \n");
  RLPStream s;
  std::vector<uint8_t> bytes;
  // Need to put a scope of vectorstream, other bytes won't get result.
  {
    vectorstream strm(bytes);
    block.serialize(strm);
  }
  if (newBlock)
    m_host.capabilityHost()->prep(_id, name(), s, NewBlockPacket, 1);
  else
    m_host.capabilityHost()->prep(_id, name(), s, BlockPacket, 1);
  s.appendList(bytes.size());
  for (auto i = 0; i < bytes.size(); i++) s << bytes[i];
  m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendBlockHash(NodeID const &_id,
                                     taraxa::DagBlock block) {
  // printf("sendBlockHash \n");
  RLPStream s;
  std::vector<uint8_t> bytes;
  m_host.capabilityHost()->prep(_id, name(), s, NewBlockHashPacket, 1);
  auto hash = block.getHash();
  s.appendList(sizeof(hash));
  for (auto i = 0; i < sizeof(hash); i++) s << ((char *)&hash)[i];
  m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlock(NodeID const &_id, blk_hash_t hash,
                                    bool newBlock) {
  printf("requestBlock %s\n", shortHash(hash.toString()).c_str());
  RLPStream s;
  std::vector<uint8_t> bytes;
  if (newBlock)
    m_host.capabilityHost()->prep(_id, name(), s, GetNewBlockPacket, 1);
  else
    m_host.capabilityHost()->prep(_id, name(), s, GetBlockPacket, 1);
  s.appendList(sizeof(hash));
  for (auto i = 0; i < sizeof(hash); i++) s << ((char *)&hash)[i];
  m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlockChildren(NodeID const &_id,
                                            blk_hash_t hash) {
  printf("Sending GetBlockChildrenPacket of block %s \n",
         shortHash(hash.toString()).c_str());
  RLPStream s;
  std::vector<uint8_t> bytes;
  m_host.capabilityHost()->prep(_id, name(), s, GetBlockChildrenPacket, 1);
  s.appendList(sizeof(hash));
  for (auto i = 0; i < sizeof(hash); i++) s << ((char *)&hash)[i];
  m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestLeaves(NodeID const &_id) {
  // printf("requestLeaves \n");
  m_peers[_id].m_state = SyncingLeaves;
  RLPStream s;
  std::vector<uint8_t> bytes;
  m_host.capabilityHost()->prep(_id, name(), s, GetLeavesPacket, 0);
  m_host.capabilityHost()->sealAndSend(_id, s);
}

std::pair<int, int> TaraxaCapability::retrieveTestData(NodeID const &_id) {
  int cnt = 0;
  int checksum = 0;
  for (auto i : m_cntReceivedMessages)
    if (_id == i.first) {
      cnt += i.second;
      checksum += m_testSums[_id];
    }

  return {cnt, checksum};
}

std::map<blk_hash_t, taraxa::DagBlock> TaraxaCapability::getBlocks() {
  return m_blocks;
}

void TaraxaCapability::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
}