#include <gtest/gtest.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>

#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>

#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "util_test/samples.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {
using namespace dev;
using namespace dev::p2p;

const unsigned NUM_TRX = 9;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

struct P2PTest : BaseTest {};

// TODO this needs to be removed and called from tracap->setPendingPeersToReady() directly
void setPendingPeersToReady(std::shared_ptr<taraxa::network::tarcap::TaraxaCapability> taraxa_capability) {
  const auto &peers_state = taraxa_capability->getPeersState();

  auto peerIds = peers_state->getAllPendingPeersIDs();
  for (const auto &peerId : peerIds) {
    auto peer = peers_state->getPendingPeer(peerId);
    if (peer) {
      peers_state->setPeerAsReadyToSendMessages(peerId, peer);
    }
  }
}

/*
Test creates one boot node and 10 nodes that uses that boot node
to find each other. Test confirm that after a delay each node had found
all other nodes.
*/
TEST_F(P2PTest, p2p_discovery) {
  auto secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                            dev::Secret::ConstructFromStringType::FromHex);
  auto key = dev::KeyPair(secret);
  const int NUMBER_OF_NODES = 40;
  dev::p2p::NetworkConfig net_conf("127.0.0.1", 20001, false, true);
  TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.is_boot_node = true;
  auto dummy_capability_constructor = [](auto /*host*/) { return Host::CapabilityList{}; };
  util::ThreadPool tp;
  auto bootHost = Host::make("TaraxaNode", dummy_capability_constructor, key, net_conf, taraxa_net_conf);
  tp.post_loop({}, [=] { bootHost->do_work(); });
  const auto &boot_node_key = bootHost->id();
  printf("Started Node id: %s\n", boot_node_key.hex().c_str());

  std::vector<std::shared_ptr<dev::p2p::Host>> nodes;
  for (int i = 0; i < NUMBER_OF_NODES; i++) {
    auto node = nodes.emplace_back(Host::make("TaraxaNode", dummy_capability_constructor, dev::KeyPair::create(),
                                              dev::p2p::NetworkConfig("127.0.0.1", 20002 + i, false, true)));
    tp.post_loop({}, [=] { node->do_work(); });
    nodes[i]->addNode(
        Node(boot_node_key, dev::p2p::NodeIPEndpoint(bi::address::from_string("127.0.0.1"), 20001, 20001)));
  }

  wait({60s, 500ms}, [&](auto &ctx) {
    for (int j = 0; j < NUMBER_OF_NODES; ++j) WAIT_EXPECT_LT(ctx, nodes[j]->getNodeCount(), NUMBER_OF_NODES / 3);
  });
}

/*
Test creates two host/network/capability and verifies that host connect
to each other and that a test packet message can be sent from one host
to the other using TaraxaCapability
*/
TEST_F(P2PTest, capability_send_test) {
  int const step = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 10007, false, true);
  prefs1.discovery = false;
  dev::p2p::NetworkConfig prefs2(localhost, 10003, false, true);
  prefs2.discovery = false;
  NetworkConfig network_conf;
  network_conf.network_transaction_interval = 1000;
  std::shared_ptr<taraxa::network::tarcap::TaraxaCapability> thc1, thc2;
  auto host1 = Host::make(
      "Test",
      [&](auto host) {
        thc1 = std::make_shared<taraxa::network::tarcap::TaraxaCapability>(host, KeyPair::create(), network_conf);
        return Host::CapabilityList{thc1};
      },
      KeyPair::create(), prefs1);
  auto host2 = Host::make(
      "Test",
      [&](auto host) {
        thc2 = std::make_shared<taraxa::network::tarcap::TaraxaCapability>(host, KeyPair::create(), network_conf);
        return Host::CapabilityList{thc2};
      },
      KeyPair::create(), prefs2);
  util::ThreadPool tp;
  tp.post_loop({}, [=] { host1->do_work(); });
  tp.post_loop({}, [=] { host2->do_work(); });
  thc1->start();
  thc2->start();

  auto port1 = host1->listenPort();
  auto port2 = host2->listenPort();
  EXPECT_NE(port1, 0);
  EXPECT_NE(port2, 0);
  EXPECT_NE(port1, port2);

  host1->addNode(
      Node(host2->id(), NodeIPEndpoint(bi::address::from_string(localhost), port2, port2), PeerType::Required));

  // Wait for up to 12 seconds, to give the hosts time to connect to each
  // other.
  for (unsigned i = 0; i < 12000; i += step) {
    std::this_thread::sleep_for(std::chrono::milliseconds(step));
    setPendingPeersToReady(thc1);
    setPendingPeersToReady(thc2);

    if ((host1->peer_count() > 0) && (host2->peer_count() > 0)) break;
  }

  EXPECT_GT(host1->peer_count(), 0);
  EXPECT_GT(host2->peer_count(), 0);

  int const target = 5;
  int checksum = 0;
  std::vector<char> dummy_data(10 * 1024 * 1024);  // 10MB memory buffer
  for (int i = 0; i < target; checksum += i++) thc2->sendTestMessage(host1->id(), i, dummy_data);

  std::this_thread::sleep_for(std::chrono::seconds(target / 2));
  std::pair<int, int> testData = thc1->retrieveTestData(host2->id());
  EXPECT_EQ(target, testData.first);
  EXPECT_EQ(checksum, testData.second);
}

/*
Test creates two host/network/capability and verifies that host connect
to each other and that a block packet message can be sent from one host
to the other using TaraxaCapability
*/

TEST_F(P2PTest, capability_send_block) {
  int const step = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 10007, false, true);
  prefs1.discovery = false;
  dev::p2p::NetworkConfig prefs2(localhost, 10003, false, true);
  prefs2.discovery = false;
  NetworkConfig network_conf;
  network_conf.network_transaction_interval = 1000;
  std::shared_ptr<taraxa::network::tarcap::TaraxaCapability> thc1, thc2;
  auto host1 = Host::make(
      "Test",
      [&](auto host) {
        thc1 = std::make_shared<taraxa::network::tarcap::TaraxaCapability>(host, KeyPair::create(), network_conf);
        return Host::CapabilityList{thc1};
      },
      KeyPair::create(), prefs1);
  auto host2 = Host::make(
      "Test",
      [&](auto host) {
        thc2 = std::make_shared<taraxa::network::tarcap::TaraxaCapability>(host, KeyPair::create(), network_conf);
        return Host::CapabilityList{thc2};
      },
      KeyPair::create(), prefs2);
  util::ThreadPool tp;
  tp.post_loop({}, [=] { host1->do_work(); });
  tp.post_loop({}, [=] { host2->do_work(); });
  thc1->start();
  thc2->start();
  auto port1 = host1->listenPort();
  auto port2 = host2->listenPort();
  EXPECT_NE(port1, 0);
  EXPECT_NE(port2, 0);
  EXPECT_NE(port1, port2);

  host1->addNode(
      Node(host2->id(), NodeIPEndpoint(bi::address::from_string(localhost), port2, port2), PeerType::Required));

  // Wait for up to 12 seconds, to give the hosts time to connect to each
  // other.
  for (unsigned i = 0; i < 12000; i += step) {
    std::this_thread::sleep_for(std::chrono::milliseconds(step));
    setPendingPeersToReady(thc1);
    setPendingPeersToReady(thc2);

    if ((host1->peer_count() > 0) && (host2->peer_count() > 0)) break;
  }

  EXPECT_GT(host1->peer_count(), 0);
  EXPECT_GT(host2->peer_count(), 0);

  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, sig_t(7777), blk_hash_t(888),
               addr_t(999));

  SharedTransactions transactions{g_signed_trx_samples[0], g_signed_trx_samples[1]};
  thc2->onNewTransactions(std::move(transactions));
  std::vector<taraxa::bytes> transactions_raw;
  transactions_raw.push_back(*g_signed_trx_samples[0]->rlp());
  transactions_raw.push_back(*g_signed_trx_samples[1]->rlp());
  thc2->sendTransactions(host1->id(), transactions_raw);
  thc2->sendBlock(host1->id(), blk, {});

  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto blocks = thc1->test_state_->getBlocks();
  auto rtransactions = thc1->test_state_->getTransactions();
  EXPECT_EQ(blocks.size(), 1);
  if (blocks.size()) {
    EXPECT_EQ(blk, blocks.begin()->second);
  }
  EXPECT_EQ(rtransactions.size(), 2);
  if (rtransactions.size() == 2) {
    EXPECT_EQ(Transaction(*transactions[0]), rtransactions[g_signed_trx_samples[0]->getHash()]);
    EXPECT_EQ(Transaction(*transactions[1]), rtransactions[g_signed_trx_samples[1]->getHash()]);
  }
}

/*
Test creates 50 host/network/capability which connect to each other
using node discovery. Block is created on one host and automatically
propagated to all other hosts. Test verifies that each node has received
the block
*/
TEST_F(P2PTest, block_propagate) {
  int const nodeCount = 10;
  const char *const localhost = "127.0.0.1";
  dev::p2p::NetworkConfig prefs1(localhost, 10007, false, true);
  std::vector<dev::p2p::NetworkConfig> vPrefs;
  for (int i = 0; i < nodeCount; i++) {
    vPrefs.push_back(dev::p2p::NetworkConfig(localhost, 10007 + i + 1, false, true));
  }
  TaraxaNetworkConfig taraxa_net_conf_1;
  taraxa_net_conf_1.is_boot_node = true;
  NetworkConfig network_conf;
  network_conf.network_transaction_interval = 1000;
  std::shared_ptr<taraxa::network::tarcap::TaraxaCapability> thc1;
  auto host1 = Host::make(
      "Test",
      [&](auto host) {
        thc1 = std::make_shared<taraxa::network::tarcap::TaraxaCapability>(host, KeyPair::create(), network_conf);
        thc1->start();
        return Host::CapabilityList{thc1};
      },
      KeyPair::create(), prefs1, taraxa_net_conf_1);
  util::ThreadPool tp;
  tp.post_loop({}, [=] { host1->do_work(); });
  std::vector<std::shared_ptr<Host>> vHosts;
  std::vector<std::shared_ptr<taraxa::network::tarcap::TaraxaCapability>> vCapabilities;
  for (int i = 0; i < nodeCount; i++) {
    auto host = vHosts.emplace_back(Host::make(
        "Test",
        [&](auto host) {
          auto cap = vCapabilities.emplace_back(
              std::make_shared<taraxa::network::tarcap::TaraxaCapability>(host, KeyPair::create(), network_conf));
          cap->start();
          return Host::CapabilityList{cap};
        },
        KeyPair::create(), vPrefs[i]));
    tp.post_loop({}, [=] { host->do_work(); });
  }
  printf("Starting %d hosts\n", nodeCount);
  auto port1 = host1->listenPort();
  EXPECT_NE(port1, 0);
  for (int i = 0; i < nodeCount; i++) {
    EXPECT_NE(vHosts[i]->listenPort(), 0);
    EXPECT_NE(port1, vHosts[i]->listenPort());
    for (int j = 0; j < i; j++) EXPECT_NE(vHosts[j]->listenPort(), vHosts[i]->listenPort());
  }

  for (int i = 0; i < nodeCount; i++) {
    if (i < 10)
      vHosts[i]->addNode(Node(host1->id(), NodeIPEndpoint(bi::address::from_string(localhost), port1, port1)));
    else
      vHosts[i]->addNode(
          Node(vHosts[i % 10]->id(), NodeIPEndpoint(bi::address::from_string(localhost), vHosts[i % 10]->listenPort(),
                                                    vHosts[i % 10]->listenPort())));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  printf("Addnode %d hosts\n", nodeCount);

  // Wait for to give the hosts time to connect to each
  // other.
  bool connected = false;
  for (unsigned i = 0; i < 500; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    connected = true;
    int counterConnected = 0;
    setPendingPeersToReady(thc1);
    for (int j = 0; j < nodeCount; j++) {
      setPendingPeersToReady(vCapabilities[j]);

      if (vHosts[j]->peer_count() < 1)
        connected = false;
      else
        counterConnected++;
    }
    // printf("Addnode %d connected\n", counterConnected);

    if ((host1->peer_count() > 0) && connected) break;
  }
  EXPECT_TRUE(connected);
  EXPECT_GT(host1->peer_count(), 0);

  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, sig_t(7777), blk_hash_t(0),
               addr_t(999));

  SharedTransactions transactions{g_signed_trx_samples[0], g_signed_trx_samples[1]};
  thc1->onNewTransactions(std::move(transactions));
  SharedTransactions transactions2;
  thc1->onNewTransactions(std::move(transactions2));
  thc1->onNewBlockReceived(std::move(blk));

  std::vector<taraxa::bytes> transactions_raw;
  transactions_raw.push_back(*g_signed_trx_samples[0]->rlp());
  transactions_raw.push_back(*g_signed_trx_samples[1]->rlp());
  for (int i = 0; i < nodeCount; i++) {
    thc1->sendTransactions(vHosts[i]->id(), transactions_raw);
  }
  for (int i = 0; i < 50; i++) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    bool synced = true;
    for (int j = 0; j < nodeCount; j++)
      if (vCapabilities[j]->test_state_->getBlocks().size() == 0) {
        synced = false;
      }
    if (synced) break;
  }
  auto blocks1 = thc1->test_state_->getBlocks();
  for (int i = 0; i < nodeCount; i++) {
    EXPECT_EQ(vCapabilities[i]->test_state_->getBlocks().size(), 1);
    if (vCapabilities[i]->test_state_->getBlocks().size() == 1) {
      EXPECT_EQ(vCapabilities[i]->test_state_->getBlocks().begin()->second, blk);
      EXPECT_EQ(vCapabilities[i]->test_state_->getBlocks().begin()->second.getHash(), blk.getHash());
    }
    auto rtransactions = vCapabilities[i]->test_state_->getTransactions();
    EXPECT_EQ(rtransactions.size(), 2);
    if (rtransactions.size() == 2) {
      EXPECT_EQ(Transaction(*transactions[0]), rtransactions[g_signed_trx_samples[0]->getHash()]);
      EXPECT_EQ(Transaction(*transactions[1]), rtransactions[g_signed_trx_samples[1]->getHash()]);
    }
  }
  EXPECT_EQ(blocks1.size(), 1);
  if (blocks1.size()) {
    EXPECT_EQ(blk, blocks1.begin()->second);
  }
}
}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
