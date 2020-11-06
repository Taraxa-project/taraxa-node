#include <gtest/gtest.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>

#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>

#include "../network.hpp"
#include "../static_init.hpp"
#include "../taraxa_capability.hpp"
#include "../util/lazy.hpp"
#include "../util_test/samples.hpp"
#include "../util_test/util.hpp"

namespace taraxa::core_tests {
using namespace dev;
using namespace dev::p2p;

const unsigned NUM_TRX = 9;
auto g_secret = Lazy(
    [] { return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex); });
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

struct P2PTest : BaseTest {};

/*
Test creates one boot node and 10 nodes that uses that boot node
to find each other. Test confirm that after a delay each node had found
all other nodes.
*/
TEST_F(P2PTest, p2p_discovery) {
  auto secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex);
  auto key = dev::KeyPair(secret);
  const int NUMBER_OF_NODES = 10;
  dev::p2p::Host bootHost("TaraxaNode", key, 20001, dev::p2p::NetworkConfig("127.0.0.1", 20001, false, true));
  bootHost.start(true);
  printf("Started Node id: %s\n", bootHost.id().hex().c_str());

  std::vector<std::shared_ptr<dev::p2p::Host>> nodes;
  for (int i = 0; i < NUMBER_OF_NODES; i++) {
    nodes.push_back(std::make_shared<dev::p2p::Host>("TaraxaNode", dev::KeyPair::create(), 20002 + i,
                                                     dev::p2p::NetworkConfig("127.0.0.1", 20002 + i, false, true)));
    nodes[i]->start();
    nodes[i]->addNode(dev::Public("7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4"
                                  "623a14a421d4"
                                  "f030cf25ef368ab394a45e920e14b57a259a09c41767dd50d1da27"
                                  "b627412a"),
                      dev::p2p::NodeIPEndpoint(bi::address::from_string("127.0.0.1"), 20001, 20001));
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  // allow more time for p2p discovery
  for (int i = 0; i < 60; i++) {
    bool allNodesFound = true;
    for (int j = 0; j < NUMBER_OF_NODES; j++)
      if (NUMBER_OF_NODES / 2 >= nodes[j]->getNodeCount()) allNodesFound = false;
    if (allNodesFound) break;
    taraxa::thisThreadSleepForSeconds(1);
  }
  for (int i = 0; i < NUMBER_OF_NODES; i++) {
    ASSERT_LT(NUMBER_OF_NODES / 3, nodes[i]->getNodeCount());
  }
  bootHost.stop();
  for (int j = 0; j < NUMBER_OF_NODES; j++) nodes[j]->stop();
}

/*
Test creates two host/network/capability and verifies that host connect
to each other and that a test packet message can be sent from one host
to the other using TaraxaCapability
*/
TEST_F(P2PTest, capability_send_test) {
  const std::string GENESIS = "0000000000000000000000000000000000000000000000000000000000000000";
  int const step = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 10002, false, true);
  dev::p2p::NetworkConfig prefs2(localhost, 10003, false, true);
  dev::p2p::Host host1("Test", 10002, prefs1);
  dev::p2p::Host host2("Test", 10003, prefs2);
  NetworkConfig network_conf;
  network_conf.network_simulated_delay = 0;
  network_conf.network_bandwidth = 40;
  network_conf.network_transaction_interval = 1000;
  auto thc1 = make_shared<TaraxaCapability>(host1, network_conf, GENESIS, false, addr_t(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                            nullptr, 2000);
  host1.registerCapability(thc1);
  auto thc2 = make_shared<TaraxaCapability>(host2, network_conf, GENESIS, false, addr_t(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                            nullptr, 2000);
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
  host1.requirePeer(host2.id(), NodeIPEndpoint(bi::address::from_string(localhost), port2, port2));

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
  for (int i = 0; i < target; checksum += i++) thc2->sendTestMessage(host1.id(), i);

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
TEST_F(P2PTest, capability_send_block) {
  const std::string GENESIS = "0000000000000000000000000000000000000000000000000000000000000000";
  int const step = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 10002, false, true);
  dev::p2p::NetworkConfig prefs2(localhost, 10003, false, true);
  dev::p2p::Host host1("Test", 10002, prefs1);
  dev::p2p::Host host2("Test", 10003, prefs2);
  NetworkConfig network_conf;
  network_conf.network_simulated_delay = 0;
  network_conf.network_bandwidth = 40;
  network_conf.network_transaction_interval = 1000;
  auto thc1 = make_shared<TaraxaCapability>(host1, network_conf, GENESIS, false, addr_t(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                            nullptr, 2000);
  host1.registerCapability(thc1);
  auto thc2 = make_shared<TaraxaCapability>(host2, network_conf, GENESIS, false, addr_t(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                            nullptr, 2000);
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
  host1.requirePeer(host2.id(), NodeIPEndpoint(bi::address::from_string(localhost), port2, port2));

  // Wait for up to 12 seconds, to give the hosts time to connect to each
  // other.
  for (unsigned i = 0; i < 12000; i += step) {
    this_thread::sleep_for(chrono::milliseconds(step));

    if ((host1.peerCount() > 0) && (host2.peerCount() > 0)) break;
  }

  EXPECT_GT(host1.peerCount(), 0);
  EXPECT_GT(host2.peerCount(), 0);

  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()}, sig_t(7777), blk_hash_t(888), addr_t(999));

  std::vector<taraxa::bytes> transactions;
  transactions.emplace_back(*g_signed_trx_samples[0].rlp());
  transactions.emplace_back(*g_signed_trx_samples[1].rlp());
  thc2->onNewTransactions(transactions, true);
  thc2->sendBlock(host1.id(), blk, true);

  this_thread::sleep_for(chrono::seconds(1));
  auto blocks = thc1->getBlocks();
  auto rtransactions = thc1->getTransactions();
  EXPECT_EQ(blocks.size(), 1);
  if (blocks.size()) EXPECT_EQ(blk, blocks.begin()->second);
  EXPECT_EQ(rtransactions.size(), 2);
  if (rtransactions.size() == 2) {
    EXPECT_EQ(Transaction(transactions[0]), rtransactions[g_signed_trx_samples[0].getHash()]);
    EXPECT_EQ(Transaction(transactions[1]), rtransactions[g_signed_trx_samples[1].getHash()]);
  }
}

/*
Test creates 50 host/network/capability which connect to each other
using node discovery. Block is created on one host and automatically
propagated to all other hosts. Test verifies that each node has received
the block
*/
TEST_F(P2PTest, block_propagate) {
  const std::string GENESIS = "0000000000000000000000000000000000000000000000000000000000000000";
  int const nodeCount = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 10002, false, true);
  std::vector<dev::p2p::NetworkConfig> vPrefs;
  for (int i = 0; i < nodeCount; i++) vPrefs.push_back(dev::p2p::NetworkConfig(localhost, 10002 + i + 1, false, true));
  dev::p2p::Host host1("Test", 10002, prefs1);
  std::vector<shared_ptr<Host>> vHosts;
  for (int i = 0; i < nodeCount; i++) vHosts.push_back(make_shared<dev::p2p::Host>("Test", 10002 + i + 1, vPrefs[i]));
  NetworkConfig network_conf;
  network_conf.network_simulated_delay = 0;
  network_conf.network_bandwidth = 40;
  network_conf.network_transaction_interval = 1000;

  auto thc1 = make_shared<TaraxaCapability>(host1, network_conf, GENESIS, false, addr_t(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                            nullptr, 2000);
  host1.registerCapability(thc1);
  std::vector<std::shared_ptr<TaraxaCapability>> vCapabilities;
  for (int i = 0; i < nodeCount; i++) {
    vCapabilities.push_back(make_shared<TaraxaCapability>(*vHosts[i], network_conf, GENESIS, false, addr_t(), nullptr, nullptr, nullptr, nullptr,
                                                          nullptr, nullptr, nullptr, 2000));
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
    for (int j = 0; j < i; j++) EXPECT_NE(vHosts[j]->listenPort(), vHosts[i]->listenPort());
  }

  for (int i = 0; i < nodeCount; i++) {
    if (i < 10)
      vHosts[i]->addNode(host1.id(), NodeIPEndpoint(bi::address::from_string(localhost), port1, port1));
    else
      vHosts[i]->addNode(vHosts[i % 10]->id(),
                         NodeIPEndpoint(bi::address::from_string(localhost), vHosts[i % 10]->listenPort(), vHosts[i % 10]->listenPort()));
    this_thread::sleep_for(chrono::milliseconds(20));
  }

  printf("Addnode %d hosts\n", nodeCount);

  bool started = true;
  for (unsigned i = 0; i < 500; i++) {
    this_thread::sleep_for(chrono::milliseconds(100));
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
  for (unsigned i = 0; i < 500; i++) {
    this_thread::sleep_for(chrono::milliseconds(100));
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

  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()}, sig_t(7777), blk_hash_t(0), addr_t(999));

  std::vector<taraxa::bytes> transactions;
  transactions.emplace_back(*g_signed_trx_samples[0].rlp());
  transactions.emplace_back(*g_signed_trx_samples[1].rlp());
  thc1->onNewTransactions(transactions, true);
  std::vector<Transaction> transactions2;
  blk.updateHash();
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
      EXPECT_EQ(vCapabilities[i]->getBlocks().begin()->second.getHash(), blk.getHash());
    }
    auto rtransactions = vCapabilities[i]->getTransactions();
    EXPECT_EQ(rtransactions.size(), 2);
    if (rtransactions.size() == 2) {
      EXPECT_EQ(Transaction(transactions[0]), rtransactions[g_signed_trx_samples[0].getHash()]);
      EXPECT_EQ(Transaction(transactions[1]), rtransactions[g_signed_trx_samples[1].getHash()]);
    }
  }
  EXPECT_EQ(blocks1.size(), 1);
  if (blocks1.size()) EXPECT_EQ(blk, blocks1.begin()->second);
}
}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  static_init();
  LoggingConfig logging;
  addr_t node_addr;
  logging.verbosity = taraxa::VerbosityError;
  setupLoggingConfiguration(node_addr, logging);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
