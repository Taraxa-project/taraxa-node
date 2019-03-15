#ifndef TARAXA_CAPABILITY_HPP
#define TARAXA_CAPABILITY_HPP
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <chrono>
#include <thread>
#include <set>
#include "state_block.hpp"
#include "full_node.hpp"
#include "visitor.hpp"

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace taraxa {

enum SubprotocolPacketType: ::byte
{
    NewBlockPacket = 0x0,
    NewBlockHashPacket,
    GetBlockPacket,
    TestPacket,
    PacketCount
};

class TaraxaPeer {
public:
    TaraxaPeer(){}
    TaraxaPeer(NodeID id):m_id(id){}

    bool isBlockKnown(blk_hash_t const& _hash) const { return m_knownBlocks.count(_hash); }
    void markBlockAsKnown(blk_hash_t const& _hash) { m_knownBlocks.insert(_hash); }
    void clearKnownBlocks() { m_knownBlocks.clear(); }

private:
    std::set<blk_hash_t> m_knownBlocks;
    NodeID m_id;
};

class TaraxaCapability : public CapabilityFace, public Worker
{
public:
    TaraxaCapability(Host const& _host) : Worker("taraxa"), m_host(_host) {
        std::random_device seed;
        m_urng = std::mt19937_64(seed());
    }

    std::string name() const override { return "taraxa"; }
    unsigned version() const override { return 1; }
    unsigned messageCount() const override { return PacketCount; }

    void onStarting() override {}
    void onStopping() override {}

    void onConnect(NodeID const& _nodeID, u256 const&) override
    {
        m_cntReceivedMessages[_nodeID] = 0;
        m_testSums[_nodeID] = 0;

        TaraxaPeer peer(_nodeID);
        m_peers.emplace(_nodeID, peer);
    }
    bool interpretCapabilityPacket(NodeID const& _nodeID, unsigned _id, RLP const& _r) override
    {
        switch(_id) {
            case NewBlockPacket: {
                //printf("Received NewBlockPacket\n");
                std::vector<::byte> blockBytes;
                for(auto i = 0; i < _r[0].itemCount(); i++) {
                    blockBytes.push_back(_r[0][i].toInt());
                }
                taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
	            StateBlock block;
	            block.deserialize(strm);

                m_peers[_nodeID].markBlockAsKnown(block.getHash());
                if(m_blocks.find(block.getHash()) == m_blocks.end()) {
                    onNewBlock(block);
                    printf("Received NewBlock\n");
                    if(full_node_) {
                        BlockVisitor visitor(full_node_);
                        taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
                        visitor.visit(strm);
                    }
                }
                break;
            }
            case NewBlockHashPacket: {
                //printf("Received NewBlockHashPacket\n");
                std::vector<::byte> blockBytes;
                for(auto i = 0; i < _r[0].itemCount(); i++) {
                    blockBytes.push_back(_r[0][i].toInt());
                }
                blk_hash_t hash;
                memcpy(&hash, blockBytes.data(), blockBytes.size());
                if(m_blocks.find(hash) == m_blocks.end() && m_blockRequestedSet.count(hash) == 0) {
                    m_blockRequestedSet.insert(hash);
                    requestBlock(_nodeID, hash);
                }
	            break;
            }
            case GetBlockPacket: {
                //printf("Received GetBlockPacket\n");
                std::vector<::byte> blockBytes;
                for(auto i = 0; i < _r[0].itemCount(); i++) {
                    blockBytes.push_back(_r[0][i].toInt());
                }
                blk_hash_t hash;
                memcpy(&hash, blockBytes.data(), blockBytes.size());
                if(m_blocks.find(hash) != m_blocks.end()) {
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
    void onDisconnect(NodeID const& _nodeID) override
    {
        m_cntReceivedMessages.erase(_nodeID);
        m_testSums.erase(_nodeID);
    }

    void sendTestMessage(NodeID const& _id, int _x)
    {
        RLPStream s;
        m_host.capabilityHost()->sealAndSend(
            _id, m_host.capabilityHost()->prep(_id, name(), s, TestPacket, 1) << _x);
    }

    vector<NodeID> selectPeers(
    std::function<bool(TaraxaPeer const&)> const& _predicate) const
    {
        vector<NodeID> allowed;
        for (auto const& peer : m_peers)
        {
            if (_predicate(peer.second))
                allowed.push_back(peer.first);
        }
        return allowed;
    }

    std::pair<std::vector<NodeID>, std::vector<NodeID>> randomPartitionPeers(
    std::vector<NodeID> const& _peers, std::size_t _number) const
    {
        vector<NodeID> part1(_peers);
        vector<NodeID> part2;

        if (_number >= _peers.size())
            return std::make_pair(part1, part2);

        std::shuffle(part1.begin(), part1.end(), m_urng);

        // Remove elements from the end of the shuffled part1 vector and move them to part2.
        std::move(part1.begin() + _number, part1.end(), std::back_inserter(part2));
        part1.erase(part1.begin() + _number, part1.end());
        return std::make_pair(move(part1), move(part2));
    }

    void onNewBlock(StateBlock block) {
        const int c_minBlockBroadcastPeers = 1;
        m_blocks[block.getHash()] = block;
        auto const peersWithoutBlock = selectPeers(
            [&](TaraxaPeer const& _peer) { return !_peer.isBlockKnown(block.getHash()); });

        auto const peersToSendNumber =
            std::max<std::size_t>(c_minBlockBroadcastPeers, std::sqrt(m_peers.size()));

        std::vector<NodeID> peersToSend;
        std::vector<NodeID> peersToAnnounce;
        std::tie(peersToSend, peersToAnnounce) =
            randomPartitionPeers(peersWithoutBlock, peersToSendNumber);


        for (NodeID const& peerID : peersToSend) {
            RLPStream ts;
            auto itPeer = m_peers.find(peerID);
            if (itPeer != m_peers.end())
            {
                sendBlock(peerID, block);
                itPeer->second.markBlockAsKnown(block.getHash());
            }
        }
        if (!peersToSend.empty())
            printf("Sent block to %lu peers\n", peersToSend.size());

        for (NodeID const& peerID : peersToAnnounce)
        {
            RLPStream ts;
            auto itPeer = m_peers.find(peerID);
            if (itPeer != m_peers.end())
            {
                sendBlockHash(peerID, block);
                itPeer->second.markBlockAsKnown(block.getHash());
            }
        }
        if (!peersToAnnounce.empty())
            printf("Anounced block to %lu peers\n", peersToAnnounce.size());
    }

    void sendBlock(NodeID const& _id, taraxa::StateBlock block) {
        //printf("sendBlock \n");
        RLPStream s;
        std::vector<uint8_t> bytes;
        // Need to put a scope of vectorstream, other bytes won't get result.
        {
            vectorstream strm(bytes);
            block.serialize(strm);
        }
        m_host.capabilityHost()->prep(_id, name(), s, NewBlockPacket, 1);
        s.appendList(bytes.size());
        for(auto i = 0; i < bytes.size(); i++)
            s << bytes[i];
        m_host.capabilityHost()->sealAndSend(
            _id, s);
    }

    void sendBlockHash(NodeID const& _id, taraxa::StateBlock block) {
        //printf("sendBlockHash \n");
        RLPStream s;
        std::vector<uint8_t> bytes;
        m_host.capabilityHost()->prep(_id, name(), s, NewBlockHashPacket, 1);
        auto hash = block.getHash();
        s.appendList(sizeof(hash));
        for(auto i = 0; i < sizeof(hash); i++)
            s << ((char *)&hash)[i];
        m_host.capabilityHost()->sealAndSend(
            _id, s);
    }

    void requestBlock(NodeID const& _id, blk_hash_t hash) {
        //printf("requestBlock \n");
        RLPStream s;
        std::vector<uint8_t> bytes;
        m_host.capabilityHost()->prep(_id, name(), s, GetBlockPacket, 1);
        s.appendList(sizeof(hash));
        for(auto i = 0; i < sizeof(hash); i++)
            s << ((char *)&hash)[i];
        m_host.capabilityHost()->sealAndSend(
            _id, s);
    }

    std::pair<int, int> retrieveTestData(NodeID const& _id)
    {
        int cnt = 0;
        int checksum = 0;
        for (auto i : m_cntReceivedMessages)
            if (_id == i.first)
            {
                cnt += i.second;
                checksum += m_testSums[_id];
            }

        return {cnt, checksum};
    }

    std::map<blk_hash_t, taraxa::StateBlock> getBlocks() {
        return m_blocks;
    }

    void setFullNode(std::shared_ptr<FullNode> full_node){ 
        full_node_ = full_node;
    }

    Host const& m_host;
    std::unordered_map<NodeID, int> m_cntReceivedMessages;
    std::unordered_map<NodeID, int> m_testSums;
    std::map<blk_hash_t, taraxa::StateBlock> m_blocks;
    std::set<blk_hash_t> m_blockRequestedSet;

    std::shared_ptr<FullNode> full_node_;

    std::unordered_map<NodeID, TaraxaPeer> m_peers;
    mutable std::mt19937_64 m_urng;  // Mersenne Twister psuedo-random number generator
};
}
#endif