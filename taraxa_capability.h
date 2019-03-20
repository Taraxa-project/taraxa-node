#ifndef TARAXA_CAPABILITY_HPP
#define TARAXA_CAPABILITY_HPP
#include "full_node.hpp"
#include "state_block.hpp"
#include "visitor.hpp"
#include <chrono>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <set>
#include <thread>

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace taraxa {

enum SubprotocolPacketType : ::byte {
	NewBlockPacket = 0x0,
	NewBlockHashPacket,
	GetBlockPacket,
	TestPacket,
	PacketCount
};

class TaraxaPeer {
  public:
	TaraxaPeer() {}
	TaraxaPeer(NodeID id) : m_id(id) {}

	bool isBlockKnown(blk_hash_t const &_hash) const {
	return m_knownBlocks.count(_hash);
	}
	void markBlockAsKnown(blk_hash_t const &_hash) {
	m_knownBlocks.insert(_hash);
	}
	void clearKnownBlocks() { m_knownBlocks.clear(); }

  private:
	std::set<blk_hash_t> m_knownBlocks;
	NodeID m_id;
};

class TaraxaCapability : public CapabilityFace, public Worker {
  public:
	TaraxaCapability(Host const &_host) : Worker("taraxa"), m_host(_host) {
	std::random_device seed;
	m_urng = std::mt19937_64(seed());
	}

	std::string name() const override { return "taraxa"; }
	unsigned version() const override { return 1; }
	unsigned messageCount() const override { return PacketCount; }

	void onStarting() override {}
	void onStopping() override {}

	void onConnect(NodeID const &_nodeID, u256 const &) override;
	bool interpretCapabilityPacket(NodeID const &_nodeID, unsigned _id,
				   RLP const &_r) override;
	void onDisconnect(NodeID const &_nodeID) override;
	void sendTestMessage(NodeID const &_id, int _x);
	void onNewBlock(StateBlock block);
	vector<NodeID>
	selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate);
	std::pair<std::vector<NodeID>, std::vector<NodeID>>
	randomPartitionPeers(std::vector<NodeID> const &_peers,
			 std::size_t _number);
	std::pair<int, int> retrieveTestData(NodeID const &_id);
	void sendBlock(NodeID const &_id, taraxa::StateBlock block);
	void sendBlockHash(NodeID const &_id, taraxa::StateBlock block);
	void requestBlock(NodeID const &_id, blk_hash_t hash);
	std::map<blk_hash_t, taraxa::StateBlock> getBlocks();
	void setFullNode(std::shared_ptr<FullNode> full_node);

	Host const &m_host;
	std::unordered_map<NodeID, int> m_cntReceivedMessages;
	std::unordered_map<NodeID, int> m_testSums;
	std::map<blk_hash_t, taraxa::StateBlock> m_blocks;
	std::set<blk_hash_t> m_blockRequestedSet;

	std::shared_ptr<FullNode> full_node_;

	std::unordered_map<NodeID, TaraxaPeer> m_peers;
	mutable std::mt19937_64
	m_urng; // Mersenne Twister psuedo-random number generator
};
} // namespace taraxa
#endif