#include <gtest/gtest.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>

#include <atomic>
#include <vector>

#include "common/static_init.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {
using namespace dev;
using namespace dev::p2p;

const unsigned NUM_TRX = 9;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

struct P2PTest : NodesTest {};

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

std::shared_ptr<taraxa::network::tarcap::TaraxaCapability> makeTarcap(std::weak_ptr<dev::p2p::Host> host,
                                                                      const dev::KeyPair &key,
                                                                      const FullNodeConfig &conf,
                                                                      const h256 &genesis_hash, unsigned version) {
  auto thread_pool = std::make_shared<network::threadpool::PacketsThreadPool>(conf.network.packets_processing_threads);
  auto packets_stats = std::make_shared<network::tarcap::TimePeriodPacketsStats>(
      conf.network.ddos_protection.packets_stats_time_period_ms, Address{});
  auto syncing_state = std::make_shared<network::tarcap::PbftSyncingState>(conf.network.deep_syncing_threshold);

  auto tarcap = std::make_shared<network::tarcap::TaraxaCapability>(version, conf, genesis_hash, host, key, thread_pool,
                                                                    packets_stats, syncing_state, nullptr, nullptr,
                                                                    nullptr, nullptr, nullptr, nullptr);

  thread_pool->startProcessing();

  return tarcap;
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

TEST_F(P2PTest, multiple_capabilities) {
  auto node_cfgs = make_node_cfgs(3);
  h256 genesis_hash;
  NetworkConfig network_conf;
  network_conf.transaction_interval_ms = 1000;
  auto cleanup = []() {
    std::filesystem::remove_all("/tmp/nw2");
    std::filesystem::remove_all("/tmp/nw3");
  };
  auto wait_for_connection = [](std::shared_ptr<Network> nw1, std::shared_ptr<Network> nw2) {
    EXPECT_HAPPENS({60s, 100ms}, [&](auto &ctx) {
      nw1->setPendingPeersToReady();
      nw2->setPendingPeersToReady();
      WAIT_EXPECT_EQ(ctx, nw1->getPeerCount(), 1)
      WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 1)
    });
  };
  const auto kp1 = KeyPair::create();
  const auto kp2 = KeyPair::create();
  cleanup();
  {
    auto nw1 =
        std::make_shared<taraxa::Network>(node_cfgs[0], genesis_hash, "/tmp/nw2", kp1, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, std::vector<network::tarcap::TarcapVersion>{3});
    auto nw2 =
        std::make_shared<taraxa::Network>(node_cfgs[1], genesis_hash, "/tmp/nw3", kp2, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, std::vector<network::tarcap::TarcapVersion>{3});

    nw1->start();
    nw2->start();
    wait_for_connection(nw1, nw2);
  }
  cleanup();
  {
    auto nw1 = std::make_shared<taraxa::Network>(node_cfgs[0], genesis_hash, "/tmp/nw2", kp1, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr,
                                                 std::vector<network::tarcap::TarcapVersion>{1, 2, 3});
    auto nw2 = std::make_shared<taraxa::Network>(node_cfgs[1], genesis_hash, "/tmp/nw3", kp2, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr,
                                                 std::vector<network::tarcap::TarcapVersion>{1, 2, 3});
    nw1->start();
    nw2->start();
    wait_for_connection(nw1, nw2);
  }
  cleanup();
  {
    auto nw1 = std::make_shared<taraxa::Network>(node_cfgs[0], genesis_hash, "/tmp/nw2", kp1, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr,
                                                 std::vector<network::tarcap::TarcapVersion>{1, 2, 3});
    auto nw2 = std::make_shared<taraxa::Network>(node_cfgs[1], genesis_hash, "/tmp/nw3", kp2, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr,
                                                 std::vector<network::tarcap::TarcapVersion>{2, 3, 4});
    nw1->start();
    nw2->start();
    wait_for_connection(nw1, nw2);
  }
  cleanup();
  {
    auto nw1 = std::make_shared<taraxa::Network>(node_cfgs[0], genesis_hash, "/tmp/nw2", kp1, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr,
                                                 std::vector<network::tarcap::TarcapVersion>{1, 2, 3});
    auto nw2 = std::make_shared<taraxa::Network>(node_cfgs[1], genesis_hash, "/tmp/nw3", kp2, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr,
                                                 std::vector<network::tarcap::TarcapVersion>{4, 5, 6});

    nw1->start();
    nw2->start();

    // check that connection wasn't established
    std::this_thread::sleep_for(5s);
    EXPECT_EQ(nw1->getPeerCount(), 0);
    EXPECT_EQ(nw2->getPeerCount(), 0);
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
