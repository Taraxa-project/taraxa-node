#include <gtest/gtest.h>
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "libp2p/Host.h"
#include "network.hpp"
#include "taraxa_capability.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace taraxa {

const unsigned NUM_TRX = 9;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_trx_samples = samples::createMockTrxSamples(0, NUM_TRX);
auto g_signed_trx_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);

/*
Test creates one boot node and 10 nodes that uses that boot node
to find each other. Test confirm that after a delay each node had found
all other nodes.
*/
TEST(p2p, p2p_discovery) {
  auto secret = dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
  auto key = dev::KeyPair(secret);
  const int NUMBER_OF_NODES = 10;
  dev::p2p::Host bootHost(
      "TaraxaNode", key,
      dev::p2p::NetworkConfig("127.0.0.1", 20001, false, true));
  bootHost.start(true);
  printf("Started Node id: %s\n", bootHost.id().hex().c_str());

  std::vector<std::shared_ptr<dev::p2p::Host>> nodes;
  for (int i = 0; i < NUMBER_OF_NODES; i++) {
    nodes.push_back(std::make_shared<dev::p2p::Host>(
        "TaraxaNode", dev::KeyPair::create(),
        dev::p2p::NetworkConfig("127.0.0.1", 20002 + i, false, true)));
    nodes[i]->start();
    nodes[i]->addNode(
        dev::Public("7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4"
                    "623a14a421d4"
                    "f030cf25ef368ab394a45e920e14b57a259a09c41767dd50d1da27"
                    "b627412a"),
        dev::p2p::NodeIPEndpoint(bi::address::from_string("127.0.0.1"), 20001,
                                 20001));
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  // allow more time for p2p discovery
  for (int i = 0; i < 20; i++) {
    bool allNodesFound = true;
    for (int j = 0; j < NUMBER_OF_NODES; j++)
      if (NUMBER_OF_NODES > nodes[j]->getNodeCount()) allNodesFound = false;
    if (allNodesFound) break;
    taraxa::thisThreadSleepForSeconds(1);
  }
  for (int i = 0; i < NUMBER_OF_NODES; i++) {
    ASSERT_EQ(NUMBER_OF_NODES, nodes[i]->getNodeCount());
  }
}

/*
Test creates two host/network/capability and verifies that host connect
to each other and that a test packet message can be sent from one host
to the other using TaraxaCapability
*/
TEST(p2p, capability_send_test) {
  int const step = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 0, false, true);
  dev::p2p::NetworkConfig prefs2(localhost, 0, false, true);
  dev::p2p::Host host1("Test", prefs1);
  dev::p2p::Host host2("Test", prefs2);
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

  for (unsigned i = 0; i < 3000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));

    if (host1.isStarted() && host2.isStarted()) break;
  }

  EXPECT_TRUE(host1.isStarted());
  EXPECT_TRUE(host2.isStarted());
  host1.requirePeer(
      host2.id(),
      NodeIPEndpoint(bi::address::from_string(localhost), port2, port2));

  // Wait for up to 12 seconds, to give the hosts time to connect to each
  // other.
  for (unsigned i = 0; i < 12000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));

    if ((host1.peerCount() > 0) && (host2.peerCount() > 0)) break;
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

/*
Test creates two host/network/capability and verifies that host connect
to each other and that a block packet message can be sent from one host
to the other using TaraxaCapability
*/
TEST(p2p, capability_send_block) {
  int const step = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 0, false, true);
  dev::p2p::NetworkConfig prefs2(localhost, 0, false, true);
  dev::p2p::Host host1("Test", prefs1);
  dev::p2p::Host host2("Test", prefs2);
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

  for (unsigned i = 0; i < 3000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));

    if (host1.isStarted() && host2.isStarted()) break;
  }

  EXPECT_TRUE(host1.isStarted());
  EXPECT_TRUE(host2.isStarted());
  host1.requirePeer(
      host2.id(),
      NodeIPEndpoint(bi::address::from_string(localhost), port2, port2));

  // Wait for up to 12 seconds, to give the hosts time to connect to each
  // other.
  for (unsigned i = 0; i < 12000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));

    if ((host1.peerCount() > 0) && (host2.peerCount() > 0)) break;
  }

  EXPECT_GT(host1.peerCount(), 0);
  EXPECT_GT(host2.peerCount(), 0);

  DagBlock blk(blk_hash_t(1111),
               {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_trx_samples[0].getHash(), g_trx_samples[1].getHash()},
               sig_t(7777), blk_hash_t(888), addr_t(999));

  std::unordered_map<trx_hash_t, Transaction> transactions;
  transactions[g_trx_samples[0].getHash()] = g_trx_samples[0];
  transactions[g_trx_samples[1].getHash()] = g_trx_samples[1];
  thc2->onNewTransactions(transactions, true);
  thc2->sendBlock(host1.id(), blk, true);

  this_thread::sleep_for(chrono::seconds(1));
  auto blocks = thc1->getBlocks();
  auto rtransactions = thc1->getTransactions();
  EXPECT_EQ(blocks.size(), 1);
  if (blocks.size()) EXPECT_EQ(blk, blocks.begin()->second);
  EXPECT_EQ(rtransactions.size(), 2);
  if (rtransactions.size() == 2) {
    EXPECT_EQ(transactions[g_trx_samples[0].getHash()],
              rtransactions[g_trx_samples[0].getHash()]);
    EXPECT_EQ(transactions[g_trx_samples[1].getHash()],
              rtransactions[g_trx_samples[1].getHash()]);
  }
}

/*
Test creates 50 host/network/capability which connect to each other
using node discovery. Block is created on one host and automatically
propagated to all other hosts. Test verifies that each node has received
the block
*/
TEST(p2p, block_propagate) {
  int const step = 10;
  int const nodeCount = 50;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 0, false, true);
  std::vector<dev::p2p::NetworkConfig> vPrefs;
  for (int i = 0; i < nodeCount; i++)
    vPrefs.push_back(dev::p2p::NetworkConfig(localhost, 0, false, true));
  dev::p2p::Host host1("Test", prefs1);
  std::vector<shared_ptr<Host>> vHosts;
  for (int i = 0; i < nodeCount; i++)
    vHosts.push_back(make_shared<dev::p2p::Host>("Test", vPrefs[i]));
  auto thc1 = make_shared<TaraxaCapability>(host1);
  host1.registerCapability(thc1);
  std::vector<std::shared_ptr<TaraxaCapability>> vCapabilities;
  for (int i = 0; i < nodeCount; i++) {
    vCapabilities.push_back(make_shared<TaraxaCapability>(*vHosts[i]));
    vHosts[i]->registerCapability(vCapabilities[i]);
  }
  host1.start(true);
  for (int i = 0; i < nodeCount; i++) {
    vHosts[i]->start();
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  printf("Starting %d hosts\n", nodeCount);
  auto port1 = host1.listenPort();
  EXPECT_NE(port1, 0);
  for (int i = 0; i < nodeCount; i++) {
    EXPECT_NE(vHosts[i]->listenPort(), 0);
    EXPECT_NE(port1, vHosts[i]->listenPort());
    for (int j = 0; j < i; j++)
      EXPECT_NE(vHosts[j]->listenPort(), vHosts[i]->listenPort());
  }

  for (int i = 0; i < nodeCount; i++) {
    if (i < 10)
      vHosts[i]->addNode(
          host1.id(),
          NodeIPEndpoint(bi::address::from_string(localhost), port1, port1));
    else
      vHosts[i]->addNode(vHosts[i % 10]->id(),
                         NodeIPEndpoint(bi::address::from_string(localhost),
                                        vHosts[i % 10]->listenPort(),
                                        vHosts[i % 10]->listenPort()));
    this_thread::sleep_for(chrono::milliseconds(20));
  }

  printf("Addnode %d hosts\n", nodeCount);

  bool started = true;
  for (unsigned i = 0; i < 10000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));
    started = true;
    for (int j = 0; j < nodeCount; j++)
      if (!vHosts[j]->isStarted()) started = false;

    if (host1.isStarted() && started) break;
  }

  EXPECT_TRUE(host1.isStarted());
  EXPECT_TRUE(started);
  printf("Started %d hosts\n", nodeCount);

  // Wait for to give the hosts time to connect to each
  // other.
  bool connected = false;
  for (unsigned i = 0; i < 50000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));
    connected = true;
    int counterConnected = 0;
    for (int j = 0; j < nodeCount; j++)
      if (vHosts[j]->peerCount() < 1)
        connected = false;
      else
        counterConnected++;
    // printf("Addnode %d connected\n", counterConnected);

    if ((host1.peerCount() > 0) && connected) break;
  }
  EXPECT_TRUE(connected);
  EXPECT_GT(host1.peerCount(), 0);

  DagBlock blk(blk_hash_t(1111),
               {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_trx_samples[0].getHash(), g_trx_samples[1].getHash()},
               sig_t(7777), blk_hash_t(888), addr_t(999));

  std::unordered_map<trx_hash_t, Transaction> transactions;
  transactions[g_trx_samples[0].getHash()] = g_trx_samples[0];
  transactions[g_trx_samples[1].getHash()] = g_trx_samples[1];
  thc1->onNewTransactions(transactions, true);
  std::vector<Transaction> transactions2;
  thc1->onNewBlockReceived(blk, transactions2);

  for (int i = 0; i < 50; i++) {
    this_thread::sleep_for(chrono::seconds(1));
    bool synced = true;
    for (int j = 0; j < nodeCount; j++)
      if (vCapabilities[j]->getBlocks().size() == 0) {
        synced = false;
      }
    if (synced) break;
  }
  auto blocks1 = thc1->getBlocks();
  for (int i = 0; i < nodeCount; i++) {
    EXPECT_EQ(vCapabilities[i]->getBlocks().size(), 1);
    if (vCapabilities[i]->getBlocks().size() == 1) {
      EXPECT_EQ(vCapabilities[i]->getBlocks().begin()->second, blk);
      EXPECT_EQ(vCapabilities[i]->getBlocks().begin()->second.getHash(),
                blk.getHash());
    }
    auto rtransactions = vCapabilities[i]->getTransactions();
    EXPECT_EQ(rtransactions.size(), 2);
    if (rtransactions.size() == 2) {
      EXPECT_EQ(transactions[g_trx_samples[0].getHash()],
                rtransactions[g_trx_samples[0].getHash()]);
      EXPECT_EQ(transactions[g_trx_samples[1].getHash()],
                rtransactions[g_trx_samples[1].getHash()]);
    }
  }
  EXPECT_EQ(blocks1.size(), 1);
  if (blocks1.size()) EXPECT_EQ(blk, blocks1.begin()->second);
}
}  // namespace taraxa

int main(int argc, char **argv) {
  LoggingOptions logOptions;
  logOptions.verbosity = VerbositySilent;
  setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
