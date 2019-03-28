/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-18 12:56:45
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-16 23:33:05
 */

#include "full_node.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "libdevcore/Log.h"
#include "network.hpp"

namespace taraxa {

/**
 * Need sequential execution
 * rocksdb cannot be opened by multiple TESTS
 */

// TEST(FullNode, send_and_receive_one_message){

// 	boost::asio::io_context context1;
// 	boost::asio::io_context context2;

// 	std::shared_ptr<FullNode> node1 (new taraxa::FullNode(context1,
// 		std::string("./core_tests/conf_full_node1.json"),
// 		std::string("./core_tests/conf_network1.json"),
// 		std::string("./core_tests/conf_rpc1.json")));
// 	// node1->setVerbose(true);
// 	node1->setDebug(true);
// 	node1->start();
// 	// send package
// 	std::shared_ptr<Network> nw2 (new taraxa::Network(context2,
// "./core_tests/conf_network2.json"));

// 	std::unique_ptr<boost::asio::io_context::work> work (new
// boost::asio::io_context::work(context1));

// 	boost::thread t([&context1](){
// 		context1.run();
// 	});

// 	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
// 	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
// 	nw2->start();

// 	DagBlock blk0 (
// 	string("1011111111111111111111111111111111111111111111111111111111111111"),
// 	{
// 	("2022222222222222222222222222222222222222222222222222222222222222"),
// 	("3033333333333333333333333333333333333333333333333333333333333333"),
// 	("4044444444444444444444444444444444444444444444444444444444444444")},
// 	{
// 	("5055555555555555555555555555555555555555555555555555555555555555"),
// 	("6066666666666666666666666666666666666666666666666666666666666666")},
// 	("70777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("8088888888888888888888888888888888888888888888888888888888888888"));

// 	nw2->sendBlock(ep, blk0);

// 	std::cout<<"Waiting packages for 1 second ..."<<std::endl;
// 	taraxa::thisThreadSleepForSeconds(1);

// 	work.reset();
// 	nw2->stop();
// 	node1->stop();

// 	ASSERT_EQ(node1->getNumReceivedBlocks(), 1);

// }

/**
 * Note: The test assume clean DB data
 */

// TEST(FullNode, send_and_receive_multiple_messages){

// 	boost::asio::io_context context1;
// 	boost::asio::io_context context2;

// 	auto node1 (std::make_shared<taraxa::FullNode>(context1,
// 		std::string("./core_tests/conf_full_node1.json"),
// 		std::string("./core_tests/conf_network1.json"),
// 		std::string("./core_tests/conf_rpc1.json")));

// 	// node1->setVerbose(true);
// 	node1->setDebug(true);
// 	node1->start();
// 	// send package
// 	auto nw2 (std::make_shared<taraxa::Network>(context2,
// "./core_tests/conf_network2.json"));

// 	std::unique_ptr<boost::asio::io_context::work> work (new
// boost::asio::io_context::work(context1));

// 	boost::thread t([&context1](){
// 		context1.run();
// 	});

// 	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
// 	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
// 	nw2->start();
// 	std::vector<DagBlock> blks;

// 	DagBlock blk1 (
// 	("0000000000000000000000000000000000000000000000000000000000000000"),
// 	{},
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000001"));

// 	DagBlock blk2 (
// 	("0000000000000000000000000000000000000000000000000000000000000000"),
// 	{},
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000002"));

// 	DagBlock blk3 (
// 	("0000000000000000000000000000000000000000000000000000000000000001"),
// 	{"0000000000000000000000000000000000000000000000000000000000000002",
// 	 "0000000000000000000000000000000000000000000000000000000000000004"
// 	},
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000003"));

// 	DagBlock blk4 (
// 	("0000000000000000000000000000000000000000000000000000000000000000"),
// 	{},
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000004"));

// 	DagBlock blk5 (
// 	("0000000000000000000000000000000000000000000000000000000000000004"),
// 	{},
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000005"));

// 	DagBlock blk6 (
// 	("0000000000000000000000000000000000000000000000000000000000000003"),
// 	{"0000000000000000000000000000000000000000000000000000000000000005"},
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000006"));

// 	blks.push_back(blk6);
// 	blks.push_back(blk5);
// 	blks.push_back(blk4);
// 	blks.push_back(blk3);
// 	blks.push_back(blk2);
// 	blks.push_back(blk1);
// 	// duplicate blocks
// 	blks.push_back(blk3);
// 	blks.push_back(blk4);
// 	blks.push_back(blk1);
// 	blks.push_back(blk2);

// 	for (auto i=0; i<blks.size(); ++i){
// 		nw2->sendBlock(ep, blks[i]);
// 	}

// 	std::cout<<"Waiting packages for 500 milliseconds ..."<<std::endl;
// 	taraxa::thisThreadSleepForMilliSeconds(500);

// 	work.reset();
// 	nw2->stop();

// 	std::cout<<"Waiting packages for 500 milliseconds ..."<<std::endl;
// 	taraxa::thisThreadSleepForMilliSeconds(500);
// 	node1->stop();

// 	// node1->drawGraph("dot.txt");
// 	EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
// 	EXPECT_EQ(node1->getNumVerticesInDag(), 7);
// 	EXPECT_EQ(node1->getNumEdgesInDag(), 9);
// 	EXPECT_EQ(node1->getNumProposedBlocks(),2);
// }

TEST(FullNode, send_and_receive_out_order_messages) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));

  // node1->setVerbose(true);
  node1->setDebug(true);
  node1->start();
  std::cout << "Node1, after start(), shared count: " << node1.use_count()
            << std::endl;
  // send package
  auto nw2(
      std::make_shared<taraxa::Network>("./core_tests/conf_network2.json"));

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  nw2->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(77777), blk_hash_t(1), name_t(16));
  DagBlock blk2(blk_hash_t(1), {}, {}, sig_t(77777), blk_hash_t(2), name_t(16));
  DagBlock blk3(blk_hash_t(2), {}, {}, sig_t(77777), blk_hash_t(3), name_t(16));
  DagBlock blk4(blk_hash_t(3), {}, {}, sig_t(77777), blk_hash_t(4), name_t(16));
  DagBlock blk5(blk_hash_t(4), {}, {}, sig_t(77777), blk_hash_t(5), name_t(16));
  DagBlock blk6(blk_hash_t(5), {blk_hash_t(4), blk_hash_t(3)}, {}, sig_t(77777),
                blk_hash_t(6), name_t(16));

  blks.emplace_back(blk6);
  blks.emplace_back(blk5);
  blks.emplace_back(blk4);
  blks.emplace_back(blk3);
  blks.emplace_back(blk2);
  blks.emplace_back(blk1);

  std::cout << "Waiting connection for 5000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(5000);

  for (auto i = 0; i < blks.size(); ++i) {
    nw2->sendBlock(node1->getNetwork()->getNodeId(), blks[i]);
  }

  std::cout << "Waiting packages for 5000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(5000);

  work.reset();
  nw2->stop();

  std::cout << "Waiting packages for 5000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(5000);
  node1->stop();
  std::cout << "Node1, after stop(), shared count: " << node1.use_count()
            << std::endl;
  t.join();

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 8);
  EXPECT_EQ(node1->getNumProposedBlocks(), 2);
}

// TEST(FullNode, send_and_receive_thousand_messages){

// 	boost::asio::io_context context1;
// 	boost::asio::io_context context2;

// 	std::shared_ptr<FullNode> node1 (new taraxa::FullNode(context1,
// 		std::string("./core_tests/conf_full_node1.json"),
// 		std::string("./core_tests/conf_network1.json"),
// 		std::string("./core_tests/conf_rpc1.json")));
// 	// node1->setVerbose(true);
// 	node1->setDebug(true);
// 	node1->start();
// 	// send package
// 	std::shared_ptr<Network> nw2 (new taraxa::Network(context2,
// "./core_tests/conf_network2.json"));

// 	std::unique_ptr<boost::asio::io_context::work> work (new
// boost::asio::io_context::work(context1));

// 	boost::thread t([&context1](){
// 		context1.run();
// 	});

// 	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
// 	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
// 	nw2->start();
// 	std::vector<DagBlock> blks;

// 	DagBlock blk0 (
// 	string("1011111111111111111111111111111111111111111111111111111111111111"),
// 	{
// 	("2022222222222222222222222222222222222222222222222222222222222222"),
// 	("3033333333333333333333333333333333333333333333333333333333333333"),
// 	("4044444444444444444444444444444444444444444444444444444444444444")},
// 	{
// 	("5055555555555555555555555555555555555555555555555555555555555555"),
// 	("6066666666666666666666666666666666666666666666666666666666666666")},
// 	("70777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("8088888888888888888888888888888888888888888888888888888888888888"));

// 	for (auto i=0; i<8000; ++i){
// 		nw2->sendBlock(ep, blk0);
// 	}

// 	std::cout<<"Waiting packages for 1 second ..."<<std::endl;
// 	taraxa::thisThreadSleepForSeconds(1);

// 	work.reset();
// 	nw2->stop();
// 	node1->stop();

// 	ASSERT_EQ(node1->getNumReceivedBlocks(), 8000);

// }

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}