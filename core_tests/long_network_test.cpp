#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "libdevcore/DBFactory.h"
#include "libdevcore/Log.h"
#include "network.hpp"

namespace taraxa {

const unsigned NUM_BLK = 100;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;

auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_blk_samples = samples::createMockDagBlkSamplesWithSignedTransactions(
    0, NUM_BLK, 0, BLK_TRX_LEN, BLK_TRX_OVERLAP, g_secret);

const unsigned NUM_TRX = 3000;
auto g_trx_samples = samples::createMockTrxSamples(0, NUM_TRX);
auto g_signed_trx_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);

FullNodeConfig g_conf1("./core_tests/conf/conf_taraxa1.json");
FullNodeConfig g_conf2("./core_tests/conf/conf_taraxa2.json");
FullNodeConfig g_conf3("./core_tests/conf/conf_taraxa3.json");

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same
*/
TEST(Network, node_sync) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  for (auto i = 0; i < NUM_BLK; ++i) {
    node1->insertBroadcastedBlockWithTransactions(g_blk_samples[i].first,
                                                  g_blk_samples[i].second);
  }

  taraxa::thisThreadSleepForMilliSeconds(10000);

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf/conf_taraxa2.json"));

  node2->setDebug(true);
  node2->start(false /*boot_node*/);

  std::cout << "Waiting Sync for max 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 200; i++) {
    taraxa::thisThreadSleepForMilliSeconds(100);
    if (node2->getNumVerticesInDag().first == NUM_BLK + 1) break;
  }
  node1->stop();
  node2->stop();

  EXPECT_EQ(node2->getNumReceivedBlocks(), NUM_BLK);
  EXPECT_EQ(node2->getNumVerticesInDag().first, NUM_BLK + 1);
}

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same using different simulated delays
*/
TEST(Network, delayed_node_sync) {
  for (int simulated_delay = 50; simulated_delay < 400; simulated_delay += 50) {
    for (int bandwidth = 10; bandwidth < 50; bandwidth += 10) {
      boost::asio::io_context context1;
      boost::asio::io_context context2;

      FullNodeConfig node1Config("./core_tests/conf/conf_taraxa1.json");
      node1Config.network.network_simulated_delay = simulated_delay;
      node1Config.network.network_bandwidth = bandwidth;
      auto node1(std::make_shared<taraxa::FullNode>(context1, node1Config));

      node1->setDebug(true);
      node1->start(true);

      // Allow node to start up
      taraxa::thisThreadSleepForMilliSeconds(1000);

      for (auto i = 0; i < 30; ++i) {
        node1->insertBroadcastedBlockWithTransactions(g_blk_samples[i].first,
                                                      g_blk_samples[i].second);
      }

      taraxa::thisThreadSleepForMilliSeconds(10000);

      FullNodeConfig node2Config("./core_tests/conf/conf_taraxa2.json");
      node2Config.network.network_simulated_delay = simulated_delay;
      node2Config.network.network_bandwidth = bandwidth;
      auto node2(std::make_shared<taraxa::FullNode>(context1, node2Config));

      std::chrono::steady_clock::time_point begin =
          std::chrono::steady_clock::now();
      node2->setDebug(true);
      node2->start(false /*boot_node*/);

      std::cout << "Waiting Sync for max 20000 milliseconds ..." << std::endl;
      for (int i = 0; i < 2000; i++) {
        taraxa::thisThreadSleepForMilliSeconds(10);
        if (node2->getNumVerticesInDag().first == NUM_BLK + 1) {
          std::chrono::steady_clock::time_point end =
              std::chrono::steady_clock::now();
          std::cout << "Ping:" << simulated_delay << "ms; "
                    << "Bandwidth:" << bandwidth
                    << "Mbs; Time to sync 100 blocks:"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           end - begin)
                           .count()
                    << "ms" << std::endl;
          break;
        }
      }
      node1->stop();
      node2->stop();

      EXPECT_EQ(node2->getNumReceivedBlocks(), 30);
      EXPECT_EQ(node2->getNumVerticesInDag().first, 30 + 1);
    }
  }
}

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same using different simulated delays
*/
TEST(Network, delayed_node_sync2) {
  boost::asio::io_context context1;

  FullNodeConfig node1Config("./core_tests/conf/conf_taraxa1.json");
  node1Config.network.network_simulated_delay = 200;
  node1Config.network.network_bandwidth = 40;
  node1Config.proposer.param1 = 1000;
  node1Config.proposer.param2 = 39000;
  auto node1(std::make_shared<taraxa::FullNode>(context1, node1Config));

  node1->setDebug(true);
  node1->start(true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<std::shared_ptr<taraxa::FullNode>> nodes;

  int num_nodes = 100;
  for (auto i = 0; i < num_nodes; i++) {
    boost::asio::io_context context2;
    FullNodeConfig node2Config("./core_tests/conf/conf_taraxa2.json");
    node2Config.network.network_simulated_delay = 200;
    node2Config.network.network_bandwidth = 40;
    node2Config.network.network_listen_port += i;
    node2Config.proposer.param1 = 1000;
    node2Config.proposer.param2 = 39000;
    node2Config.db_path += std::to_string(i);
    nodes.push_back(std::make_shared<taraxa::FullNode>(context2, node2Config));
    nodes[i]->start(false);
    taraxa::thisThreadSleepForMilliSeconds(10);
  }

  while (true) {
    bool all_connected = true;
    for (auto i = 0; i < num_nodes; i++) {
      if (nodes[i]->getPeerCount() < 1) {
        all_connected = false;
        printf("Node not connected %d\n", i);
        taraxa::thisThreadSleepForMilliSeconds(10000);
        break;
      }
    }
    if (all_connected) {
      printf("Nodes connected\n");
      break;
    }
  }

  printf("Storing transactions\n");
  for (auto i = 0; i < NUM_TRX; i++) {
    nodes[i % num_nodes]->insertTransaction(g_signed_trx_samples[i]);
  }
  printf("Stored transactions\n");

  taraxa::thisThreadSleepForMilliSeconds(100000);
}  // namespace taraxa

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same using different simulated delays
*/
TEST(Network, delayed_node_sync3) {
  boost::asio::io_context context1;

  FullNodeConfig node1Config("./core_tests/conf/conf_taraxa1.json");
  node1Config.network.network_simulated_delay = 200;
  node1Config.network.network_bandwidth = 40;
  node1Config.proposer.param1 = 100000;
  node1Config.proposer.param2 = 390000;
  auto node1(std::make_shared<taraxa::FullNode>(context1, node1Config));

  node1->setDebug(true);
  node1->start(true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<std::shared_ptr<taraxa::FullNode>> nodes;

  int num_nodes = 20;
  for (auto i = 0; i < num_nodes; i++) {
    boost::asio::io_context context2;
    FullNodeConfig node2Config("./core_tests/conf/conf_taraxa2.json");
    node2Config.network.network_simulated_delay = 200;
    node2Config.network.network_bandwidth = 40;
    node2Config.network.network_listen_port += i;
    node2Config.proposer.param1 = 100000;
    node2Config.proposer.param2 = 390000;
    node2Config.db_path += std::to_string(i);
    nodes.push_back(std::make_shared<taraxa::FullNode>(context2, node2Config));
    nodes[i]->start(false);
    taraxa::thisThreadSleepForMilliSeconds(10);
  }

  while (true) {
    bool all_connected = true;
    for (auto i = 0; i < num_nodes; i++) {
      if (nodes[i]->getPeerCount() < 1) {
        all_connected = false;
        printf("Node not connected %d\n", i);
        taraxa::thisThreadSleepForMilliSeconds(10000);
        break;
      }
    }
    if (all_connected) {
      printf("Nodes connected\n");
      break;
    }
  }

  printf("Storing transactions\n");
  for (auto i = 0; i < NUM_TRX; i++) {
    nodes[i % num_nodes]->insertTransaction(g_signed_trx_samples[i]);
  }
  printf("Stored transactions\n");

  while (true) {
    int totalTrx = 0;
    for (int i = 0; i < num_nodes; i++) {
      totalTrx += nodes[i]->getVerifiedTrxSnapShot().size();
      if (i % 10 == 0)
        printf("%d %lu\n", i,
               nodes[i]->getVerifiedTrxSnapShot().size());
    }
    printf("Total: %d\n", totalTrx);
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
}
}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityInfo;
  // logOptions.includeChannels.push_back("NETWORK");
  // logOptions.includeChannels.push_back("DAGMGR");
  // logOptions.includeChannels.push_back("TARCAP");
  logOptions.includeChannels.push_back("TMSTM");
  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
