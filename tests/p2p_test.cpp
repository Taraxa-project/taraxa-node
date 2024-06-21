#include <gtest/gtest.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>

#include <atomic>
#include <vector>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "network/tarcap/tarcap_version.hpp"
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

class TestTaraxaCapability final : public dev::p2p::CapabilityFace {
 public:
  TestTaraxaCapability(taraxa::network::tarcap::TarcapVersion version) : version_(version) {}

  std::string name() const override { return ""; }
  taraxa::network::tarcap::TarcapVersion version() const override { return version_; }
  unsigned messageCount() const override { return 0; }
  void onConnect(std::weak_ptr<dev::p2p::Session>, u256 const &) override {}
  void onDisconnect(dev::p2p::NodeID const &) override {}
  void interpretCapabilityPacket(std::weak_ptr<dev::p2p::Session>, unsigned, dev::RLP const &) override {}
  std::string packetTypeToString(unsigned) const override { return ""; }

 private:
  taraxa::network::tarcap::TarcapVersion version_{1};
};

std::shared_ptr<dev::p2p::Host> makeTestNode(unsigned short listenPort,
                                             std::vector<taraxa::network::tarcap::TarcapVersion> tarcap_versions,
                                             std::filesystem::path state_file_path) {
  auto makeTestTarcaps = [tarcap_versions](std::weak_ptr<dev::p2p::Host>) {
    Host::CapabilityList tarcaps;
    for (const auto &version : tarcap_versions) {
      tarcaps.emplace_back(std::make_shared<TestTaraxaCapability>(version));
    }

    return tarcaps;
  };

  return Host::make("TaraxaNode", makeTestTarcaps, dev::KeyPair::create(),
                    dev::p2p::NetworkConfig("127.0.0.1", listenPort, false, true), TaraxaNetworkConfig{},
                    state_file_path);
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
  // Create boot node
  auto secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                            dev::Secret::ConstructFromStringType::FromHex);
  auto key = dev::KeyPair(secret);
  dev::p2p::NetworkConfig net_conf("127.0.0.1", 20001, false, true);
  TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.is_boot_node = true;
  auto boot_node = Host::make(
      "TaraxaNode", [](auto /*host*/) { return Host::CapabilityList{}; }, key, net_conf, taraxa_net_conf);
  const auto &boot_node_key = boot_node->id();

  util::ThreadPool boot_node_tp;
  boot_node_tp.post_loop({}, [=] { boot_node->do_work(); });

  // Create 2 nodes(hosts) with specified tarcap versionS and wait for their connection being established
  auto test_tarcaps = [boot_node_key](std::vector<taraxa::network::tarcap::TarcapVersion> node1_tarcap_versions,
                                      std::vector<taraxa::network::tarcap::TarcapVersion> node2_tarcap_versions,
                                      bool wait_for_connection = true) -> std::vector<std::shared_ptr<dev::p2p::Host>> {
    std::filesystem::remove_all("/tmp/nw1");
    std::filesystem::remove_all("/tmp/nw2");

    util::ThreadPool tp;

    const auto node1 = makeTestNode(20002, node1_tarcap_versions, "/tmp/nw1");
    std::cout << "node1->peer_count(): " << node1->peer_count() << std::endl;
    node1->addNode(Node(boot_node_key, dev::p2p::NodeIPEndpoint(bi::address::from_string("127.0.0.1"), 20001, 20001)));
    tp.post_loop({}, [=] { node1->do_work(); });

    const auto node2 = makeTestNode(20003, node2_tarcap_versions, "/tmp/nw2");
    node2->addNode(Node(boot_node_key, dev::p2p::NodeIPEndpoint(bi::address::from_string("127.0.0.1"), 20001, 20001)));
    tp.post_loop({}, [=] { node2->do_work(); });

    if (wait_for_connection) {
      wait({60s, 500ms}, [&](auto &ctx) {
        WAIT_EXPECT_EQ(ctx, node1->peer_count(), 1 /* boot node + node2 */);
        WAIT_EXPECT_EQ(ctx, node2->peer_count(), 1 /* boot node + node1 */);
      });

      return {nullptr, nullptr};
    }

    return {node1, node2};
  };

  // At least 1 common tarcap version - connection should be established
  { test_tarcaps({1}, {1}); }
  { test_tarcaps({1, 2, 3}, {3, 4, 5}); }

  // No common tarcapm version, connection should not be established
  {
    auto nodes = test_tarcaps({1, 2, 3}, {4, 5, 6}, false);
    // check that connection wasn't established
    std::this_thread::sleep_for(5s);
    EXPECT_EQ(nodes[0]->peer_count(), 0);
    EXPECT_EQ(nodes[1]->peer_count(), 0);
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
