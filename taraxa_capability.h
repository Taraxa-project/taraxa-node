#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <chrono>
#include <thread>
#include "state_block.hpp"

using namespace std;
using namespace dev;
using namespace dev::p2p;
using namespace taraxa;

enum SubprotocolPacketType: ::byte
{
    BlockPacket = 0x0,
    TestPacket,
    PacketCount
};

class TaraxaCapability : public CapabilityFace, public Worker
{
public:
    TaraxaCapability(Host const& _host) : Worker("taraxa"), m_host(_host) {}

    std::string name() const override { return "taraxa"; }
    unsigned version() const override { return 1; }
    unsigned messageCount() const override { return PacketCount; }

    void onStarting() override {}
    void onStopping() override {}

    void onConnect(NodeID const& _nodeID, u256 const&) override
    {
        m_cntReceivedMessages[_nodeID] = 0;
        m_testSums[_nodeID] = 0;
    }
    bool interpretCapabilityPacket(NodeID const& _nodeID, unsigned _id, RLP const& _r) override
    {
        switch(_id) {
            case BlockPacket: {
                std::vector<::byte> blockBytes;
                for(auto i = 0; i < _r[0].itemCount(); i++) {
                    blockBytes.push_back(_r[0][i].toInt());
                }
                taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
	            StateBlock block;
	            block.deserialize(strm);
                m_receivedBlocks.push_back(block);
                break;
            }
            case TestPacket:
                ++m_cntReceivedMessages[_nodeID];
                m_testSums[_nodeID] += _r[0].toInt();
                BOOST_ASSERT(_id == TestPacket);
                return (_id == TestPacket);
        }; 
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

    void sendBlock(NodeID const& _id, taraxa::StateBlock block) {
        RLPStream s;
        std::vector<uint8_t> bytes;
        // Need to put a scope of vectorstream, other bytes won't get result.
        {
            vectorstream strm(bytes);
            block.serialize(strm);
        }
        m_host.capabilityHost()->prep(_id, name(), s, BlockPacket, 1);
        s.appendList(bytes.size());
        for(auto i = 0; i < bytes.size(); i++)
            s << bytes[i];
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

    std::vector<taraxa::StateBlock> getBlocks() {
        return m_receivedBlocks;
    }

    Host const& m_host;
    std::unordered_map<NodeID, int> m_cntReceivedMessages;
    std::unordered_map<NodeID, int> m_testSums;
    std::vector<taraxa::StateBlock> m_receivedBlocks;
};