#include "taraxa_capability.h"

using namespace taraxa;

void TaraxaCapability::onConnect(NodeID const &_nodeID, u256 const &) {
    m_cntReceivedMessages[_nodeID] = 0;
    m_testSums[_nodeID] = 0;

    TaraxaPeer peer(_nodeID);
    m_peers.emplace(_nodeID, peer);
}
bool TaraxaCapability::interpretCapabilityPacket(NodeID const &_nodeID,
						 unsigned _id, RLP const &_r) {
    switch (_id) {
    case NewBlockPacket: {
	// printf("Received NewBlockPacket\n");
	std::vector<::byte> blockBytes;
	for (auto i = 0; i < _r[0].itemCount(); i++) {
	    blockBytes.push_back(_r[0][i].toInt());
	}
	taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
	StateBlock block;
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
    case NewBlockHashPacket: {
	// printf("Received NewBlockHashPacket\n");
	std::vector<::byte> blockBytes;
	for (auto i = 0; i < _r[0].itemCount(); i++) {
	    blockBytes.push_back(_r[0][i].toInt());
	}
	blk_hash_t hash;
	memcpy(&hash, blockBytes.data(), blockBytes.size());
	if (m_blocks.find(hash) == m_blocks.end() &&
	    m_blockRequestedSet.count(hash) == 0) {
	    m_blockRequestedSet.insert(hash);
	    requestBlock(_nodeID, hash);
	}
	break;
    }
    case GetBlockPacket: {
	// printf("Received GetBlockPacket\n");
	std::vector<::byte> blockBytes;
	for (auto i = 0; i < _r[0].itemCount(); i++) {
	    blockBytes.push_back(_r[0][i].toInt());
	}
	blk_hash_t hash;
	memcpy(&hash, blockBytes.data(), blockBytes.size());
	if (m_blocks.find(hash) != m_blocks.end()) {
	    sendBlock(_nodeID, m_blocks[hash]);
	}
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
	_id, m_host.capabilityHost()->prep(_id, name(), s, TestPacket, 1)
		 << _x);
}

vector<NodeID> TaraxaCapability::selectPeers(
    std::function<bool(TaraxaPeer const &)> const &_predicate) {
    vector<NodeID> allowed;
    for (auto const &peer : m_peers) {
	if (_predicate(peer.second))
	    allowed.push_back(peer.first);
    }
    return allowed;
}

std::pair<std::vector<NodeID>, std::vector<NodeID>>
TaraxaCapability::randomPartitionPeers(std::vector<NodeID> const &_peers,
				       std::size_t _number) {
    vector<NodeID> part1(_peers);
    vector<NodeID> part2;

    if (_number >= _peers.size())
	return std::make_pair(part1, part2);

    std::shuffle(part1.begin(), part1.end(), m_urng);

    // Remove elements from the end of the shuffled part1 vector and move them
    // to part2.
    std::move(part1.begin() + _number, part1.end(), std::back_inserter(part2));
    part1.erase(part1.begin() + _number, part1.end());
    return std::make_pair(move(part1), move(part2));
}

void TaraxaCapability::onNewBlock(StateBlock block) {
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
	    sendBlock(peerID, block);
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

void TaraxaCapability::sendBlock(NodeID const &_id, taraxa::StateBlock block) {
    // printf("sendBlock \n");
    RLPStream s;
    std::vector<uint8_t> bytes;
    // Need to put a scope of vectorstream, other bytes won't get result.
    {
	vectorstream strm(bytes);
	block.serialize(strm);
    }
    m_host.capabilityHost()->prep(_id, name(), s, NewBlockPacket, 1);
    s.appendList(bytes.size());
    for (auto i = 0; i < bytes.size(); i++)
	s << bytes[i];
    m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::sendBlockHash(NodeID const &_id,
				     taraxa::StateBlock block) {
    // printf("sendBlockHash \n");
    RLPStream s;
    std::vector<uint8_t> bytes;
    m_host.capabilityHost()->prep(_id, name(), s, NewBlockHashPacket, 1);
    auto hash = block.getHash();
    s.appendList(sizeof(hash));
    for (auto i = 0; i < sizeof(hash); i++)
	s << ((char *)&hash)[i];
    m_host.capabilityHost()->sealAndSend(_id, s);
}

void TaraxaCapability::requestBlock(NodeID const &_id, blk_hash_t hash) {
    // printf("requestBlock \n");
    RLPStream s;
    std::vector<uint8_t> bytes;
    m_host.capabilityHost()->prep(_id, name(), s, GetBlockPacket, 1);
    s.appendList(sizeof(hash));
    for (auto i = 0; i < sizeof(hash); i++)
	s << ((char *)&hash)[i];
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

std::map<blk_hash_t, taraxa::StateBlock> TaraxaCapability::getBlocks() {
    return m_blocks;
}

void TaraxaCapability::setFullNode(std::shared_ptr<FullNode> full_node) {
    full_node_ = full_node;
}