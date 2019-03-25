/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-28 11:12:22
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 21:24:34
 */

#include "network.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "libdevcore/Log.h"

namespace taraxa {

TEST(Network, transfer_block) {
  std::shared_ptr<Network> nw1(
      new taraxa::Network("./core_tests/conf_network1.json"));
  std::shared_ptr<Network> nw2(
      new taraxa::Network("./core_tests/conf_network2.json"));

  nw1->start();
  nw2->start();
  DagBlock blk(blk_hash_t(1111),
               {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {trx_hash_t(555), trx_hash_t(666)}, sig_t(7777), blk_hash_t(888),
               name_t(999));

  taraxa::thisThreadSleepForSeconds(10);

  for (auto i = 0; i < 1; ++i) {
    nw2->sendBlock(nw1->getNodeId(), blk, true);
  }

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  taraxa::thisThreadSleepForSeconds(10);

  nw2->stop();
  nw1->stop();
  unsigned long long num_received = nw1->getReceivedBlocksCount();
  ASSERT_EQ(1, num_received);
}

TEST(Network, save_network) {
	{
		std::shared_ptr<Network> nw1(
		    new taraxa::Network("./core_tests/conf_network1.json"));
		std::shared_ptr<Network> nw2(
		    new taraxa::Network("./core_tests/conf_network2.json"));
		std::shared_ptr<Network> nw3(
		    new taraxa::Network("./core_tests/conf_network3.json"));

		nw1->start();
		nw2->start();
		nw3->start();

		taraxa::thisThreadSleepForSeconds(20);

		ASSERT_EQ(2, nw1->getPeerCount());
		ASSERT_EQ(2, nw2->getPeerCount());
		ASSERT_EQ(2, nw3->getPeerCount());

		nw1->stop();
		nw2->stop();
		nw3->stop();
		taraxa::thisThreadSleepForSeconds(10);
		nw2->saveNetwork("/tmp/nw2");
		nw3->saveNetwork("/tmp/nw3");
	}

	std::shared_ptr<Network> nw2(
	    new taraxa::Network("./core_tests/conf_network2.json", "/tmp/nw2"));
	std::shared_ptr<Network> nw3(
	    new taraxa::Network("./core_tests/conf_network3.json", "/tmp/nw3"));
	nw2->start();
	nw3->start();

	taraxa::thisThreadSleepForSeconds(10);

	ASSERT_EQ(1, nw2->getPeerCount());
	ASSERT_EQ(1, nw3->getPeerCount());
}

TEST(Network, node_sync) {
	boost::asio::io_context context1;
	boost::asio::io_context context2;

	auto node1(std::make_shared<taraxa::FullNode>(
	    context1, std::string("./core_tests/conf_full_node1.json"),
	    std::string("./core_tests/conf_network1.json")));

	node1->setDebug(true);
	node1->start();
	std::vector<DagBlock> blks;

	DagBlock blk1(
	    "0000000000000000000000000000000000000000000000000000000000000000",
	    {}, {},
	    "777777777777777777777777777777777777777777777777777777777777777777"
	    "777777"
	    "77777777777777777777777777777777777777777777777777777777",
	    "0000000000000000000000000000000000000000000000000000000000000001",
	    "000000000000000000000000000000000000000000000000000000000000000F");

	DagBlock blk2(
	    "0000000000000000000000000000000000000000000000000000000000000001",
	    {}, {},
	    "777777777777777777777777777777777777777777777777777777777777777777"
	    "777777"
	    "77777777777777777777777777777777777777777777777777777777",
	    "0000000000000000000000000000000000000000000000000000000000000002",
	    "000000000000000000000000000000000000000000000000000000000000000F");

	DagBlock blk3(
	    "0000000000000000000000000000000000000000000000000000000000000002",
	    {}, {},
	    "777777777777777777777777777777777777777777777777777777777777777777"
	    "777777"
	    "77777777777777777777777777777777777777777777777777777777",
	    "0000000000000000000000000000000000000000000000000000000000000003",
	    "000000000000000000000000000000000000000000000000000000000000000F");

	DagBlock blk4(
	    "0000000000000000000000000000000000000000000000000000000000000003",
	    {}, {},
	    "777777777777777777777777777777777777777777777777777777777777777777"
	    "777777"
	    "77777777777777777777777777777777777777777777777777777777",
	    "0000000000000000000000000000000000000000000000000000000000000004",
	    "000000000000000000000000000000000000000000000000000000000000000F");

	DagBlock blk5(
	    "0000000000000000000000000000000000000000000000000000000000000004",
	    {}, {},
	    "777777777777777777777777777777777777777777777777777777777777777777"
	    "777777"
	    "77777777777777777777777777777777777777777777777777777777",
	    "0000000000000000000000000000000000000000000000000000000000000005",
	    "000000000000000000000000000000000000000000000000000000000000000F");

	DagBlock blk6(
	    "0000000000000000000000000000000000000000000000000000000000000005",
	    {"0000000000000000000000000000000000000000000000000000000000000004",
	     "000000000000000000000000000000000000000000000000000000000000000"
	     "3"},
	    {},
	    "777777777777777777777777777777777777777777777777777777777777777777"
	    "777777"
	    "77777777777777777777777777777777777777777777777777777777",
	    "0000000000000000000000000000000000000000000000000000000000000006",
	    "000000000000000000000000000000000000000000000000000000000000000F");

	blks.push_back(blk6);
	blks.push_back(blk5);
	blks.push_back(blk4);
	blks.push_back(blk3);
	blks.push_back(blk2);
	blks.push_back(blk1);

	for (auto i = 0; i < blks.size(); ++i) {
		node1->storeBlock(blks[i]);
	}

	auto node2(std::make_shared<taraxa::FullNode>(
	    context1, std::string("./core_tests/conf_full_node2.json"),
	    std::string("./core_tests/conf_network2.json")));

	node2->setDebug(true);
	node2->start();

	std::cout << "Waiting Sync for 5000 milliseconds ..." << std::endl;
	taraxa::thisThreadSleepForMilliSeconds(5000);

	node1->stop();
	node2->stop();

	// node1->drawGraph("dot.txt");
	EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
	EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
	EXPECT_EQ(node1->getNumEdgesInDag().first, 8);

	EXPECT_EQ(node2->getNumReceivedBlocks(), blks.size());
	EXPECT_EQ(node2->getNumVerticesInDag().first, 7);
	EXPECT_EQ(node2->getNumEdgesInDag().first, 8);
}

}  // namespace taraxa

int main(int argc, char **argv) {
	dev::LoggingOptions logOptions;
	logOptions.verbosity = dev::VerbositySilent;
	dev::setupLogging(logOptions);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}