/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-28 11:12:22 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 21:20:14
 */
 
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <vector>
#include <atomic>
#include <iostream>
#include "libp2p/Host.h"
#include <libp2p/Network.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Session.h>
#include <libdevcore/Log.h>
#include "network.hpp"
#include "taraxa_capability.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace taraxa {

TEST(p2p, p2p_discovery){
	auto secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex);
	auto key = dev::KeyPair(secret);
	const int NUMBER_OF_NODES = 10;
	dev::p2p::Host bootHost("TaraxaNode", key, dev::p2p::NetworkConfig("127.0.0.1", 20001, false, true));
	bootHost.start();
	printf("Started Node id: %s\n", bootHost.id().hex().c_str());

	std::vector<std::shared_ptr<dev::p2p::Host> > nodes;
	for(int i = 0; i < NUMBER_OF_NODES; i++) {
		nodes.push_back(std::make_shared<dev::p2p::Host>("TaraxaNode", dev::KeyPair::create(), dev::p2p::NetworkConfig("127.0.0.1", 20002 + i, false, true)));
		nodes[i]->start();
        nodes[i]->addNode(dev::Public("7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4623a14a421d4f030cf25ef368ab394a45e920e14b57a259a09c41767dd50d1da27b627412a"), dev::p2p::NodeIPEndpoint(bi::address::from_string("127.0.0.1"), 20001, 20001));
		taraxa::thisThreadSleepForMilliSeconds(100);
	}
    // allow more time for p2p discovery
	taraxa::thisThreadSleepForSeconds(12);
	for(int i = 0; i < NUMBER_OF_NODES; i++) {
        ASSERT_EQ(NUMBER_OF_NODES, nodes[i]->getNodeCount());
	}
}

TEST(p2p, capability)
{
    int const step = 10;
    const char* const localhost = "127.0.0.1";
    NetworkConfig prefs1(localhost, 0, false , true );
    NetworkConfig prefs2(localhost, 0, false , true );
    Host host1("Test", prefs1);
    Host host2("Test", prefs2);
    auto thc1 = make_shared<TaraxaCapability>(host1);
    host1.registerCapability(thc1);
    auto thc2 = make_shared<TaraxaCapability>(host2);
    host2.registerCapability(thc2);
    host1.start();
    host2.start();
    auto port1 = host1.listenPort();
    auto port2 = host2.listenPort();
    EXPECT_NE(port1, 0);
    EXPECT_NE(port2, 0);
    EXPECT_NE(port1, port2);

    for (unsigned i = 0; i < 3000; i += step)
    {
        this_thread::sleep_for(chrono::milliseconds(step));

        if (host1.isStarted() && host2.isStarted())
            break;
    }

    EXPECT_TRUE(host1.isStarted());
    EXPECT_TRUE(host2.isStarted());
    host1.requirePeer(
        host2.id(), NodeIPEndpoint(bi::address::from_string(localhost), port2, port2));

    // Wait for up to 12 seconds, to give the hosts time to connect to each other.
    for (unsigned i = 0; i < 12000; i += step)
    {
        this_thread::sleep_for(chrono::milliseconds(step));

        if ((host1.peerCount() > 0) && (host2.peerCount() > 0))
            break;
    }

    EXPECT_GT(host1.peerCount(), 0);
    EXPECT_GT(host2.peerCount(), 0);

    int const target = 64;
    int checksum = 0;
    for (int i = 0; i < target; checksum += i++)
        thc2->sendTestMessage(host1.id(), i);

    this_thread::sleep_for(chrono::seconds(target / 64 + 1));
    std::pair<int, int> testData = thc1->retrieveTestData(host2.id());
    EXPECT_EQ(target, testData.first);
    EXPECT_EQ(checksum, testData.second);
}


TEST(p2p, block)
{
    int const step = 10;
    const char* const localhost = "127.0.0.1";
    NetworkConfig prefs1(localhost, 0, false /* upnp */, true /* allow local discovery */);
    NetworkConfig prefs2(localhost, 0, false /* upnp */, true /* allow local discovery */);
    Host host1("Test", prefs1);
    Host host2("Test", prefs2);
    auto thc1 = make_shared<TaraxaCapability>(host1);
    host1.registerCapability(thc1);
    auto thc2 = make_shared<TaraxaCapability>(host2);
    host2.registerCapability(thc2);
    host1.start();
    host2.start();
    auto port1 = host1.listenPort();
    auto port2 = host2.listenPort();
    EXPECT_NE(port1, 0);
    EXPECT_NE(port2, 0);
    EXPECT_NE(port1, port2);

    for (unsigned i = 0; i < 3000; i += step)
    {
        this_thread::sleep_for(chrono::milliseconds(step));

        if (host1.isStarted() && host2.isStarted())
            break;
    }

    EXPECT_TRUE(host1.isStarted());
    EXPECT_TRUE(host2.isStarted());
    host1.requirePeer(
        host2.id(), NodeIPEndpoint(bi::address::from_string(localhost), port2, port2));

    // Wait for up to 12 seconds, to give the hosts time to connect to each other.
    for (unsigned i = 0; i < 12000; i += step)
    {
        this_thread::sleep_for(chrono::milliseconds(step));

        if ((host1.peerCount() > 0) && (host2.peerCount() > 0))
            break;
    }

    EXPECT_GT(host1.peerCount(), 0);
    EXPECT_GT(host2.peerCount(), 0);

    StateBlock blk (
	"1111111111111111111111111111111111111111111111111111111111111111",
	{
	"2222222222222222222222222222222222222222222222222222222222222222",
	"3333333333333333333333333333333333333333333333333333333333333333",
	"4444444444444444444444444444444444444444444444444444444444444444"}, 
	{
	"5555555555555555555555555555555555555555555555555555555555555555",
	"6666666666666666666666666666666666666666666666666666666666666666"},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"8888888888888888888888888888888888888888888888888888888888888888",
	"000000000000000000000000000000000000000000000000000000000000000F");

	thc2->sendBlock(host1.id(), blk);

	this_thread::sleep_for(chrono::seconds(1));
	auto blocks = thc1->getBlocks();
	EXPECT_EQ(blocks.size(), 1);
	EXPECT_EQ(blk, blocks[0]);
}
}  // namespace taraxa

int main(int argc, char** argv){
	LoggingOptions logOptions;
	logOptions.verbosity = VerbositySilent;
	setupLogging(logOptions);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}