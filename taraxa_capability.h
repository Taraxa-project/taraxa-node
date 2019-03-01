#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <chrono>
#include <thread>

using namespace std;
using namespace dev;
using namespace dev::p2p;

class TaraxaCapability : public CapabilityFace, public Worker
{
public:
    TaraxaCapability(Host const& _host) : Worker("taraxa"), m_host(_host) {}

    std::string name() const override { return "taraxa"; }
    unsigned version() const override { return 1; }
    unsigned messageCount() const override { return UserPacket + 1; }

    void onStarting() override {}
    void onStopping() override {}

    void onConnect(NodeID const& _nodeID, u256 const&) override
    {
        m_cntReceivedMessages[_nodeID] = 0;
        m_testSums[_nodeID] = 0;
    }
    bool interpretCapabilityPacket(NodeID const& _nodeID, unsigned _id, RLP const& _r) override
    {
        ++m_cntReceivedMessages[_nodeID];
        m_testSums[_nodeID] += _r[0].toInt();
        BOOST_ASSERT(_id == UserPacket);
        return (_id == UserPacket);
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
            _id, m_host.capabilityHost()->prep(_id, name(), s, UserPacket, 1) << _x);
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

    Host const& m_host;
    std::unordered_map<NodeID, int> m_cntReceivedMessages;
    std::unordered_map<NodeID, int> m_testSums;
};