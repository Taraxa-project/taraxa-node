/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-18 12:56:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-30 10:54:10
 */
 
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <vector>
#include <atomic>
#include <iostream>
#include "network.hpp"
#include "full_node.hpp"

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
// 	std::shared_ptr<Network> nw2 (new taraxa::Network(context2, "./core_tests/conf_network2.json"));

// 	std::unique_ptr<boost::asio::io_context::work> work (new boost::asio::io_context::work(context1));

// 	boost::thread t([&context1](){
// 		context1.run();
// 	});

// 	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
// 	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
// 	nw2->start();

// 	StateBlock blk0 (
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
// 	auto nw2 (std::make_shared<taraxa::Network>(context2, "./core_tests/conf_network2.json"));

// 	std::unique_ptr<boost::asio::io_context::work> work (new boost::asio::io_context::work(context1));

// 	boost::thread t([&context1](){
// 		context1.run();
// 	});

// 	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
// 	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
// 	nw2->start();
// 	std::vector<StateBlock> blks;

// 	StateBlock blk1 (
// 	("0000000000000000000000000000000000000000000000000000000000000000"),
// 	{}, 
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000001"));

// 	StateBlock blk2 (
// 	("0000000000000000000000000000000000000000000000000000000000000000"),
// 	{}, 
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000002"));
	
// 	StateBlock blk3 (
// 	("0000000000000000000000000000000000000000000000000000000000000001"),
// 	{"0000000000000000000000000000000000000000000000000000000000000002",
// 	 "0000000000000000000000000000000000000000000000000000000000000004"
// 	}, 
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000003"));

// 	StateBlock blk4 (
// 	("0000000000000000000000000000000000000000000000000000000000000000"),
// 	{}, 
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000004"));
	
// 	StateBlock blk5 (
// 	("0000000000000000000000000000000000000000000000000000000000000004"),
// 	{}, 
// 	{},
// 	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
// 	("0000000000000000000000000000000000000000000000000000000000000005"));
	
// 	StateBlock blk6 (
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


TEST(FullNode, send_and_receive_out_order_messages){

	boost::asio::io_context context1;
	boost::asio::io_context context2;

	auto node1 (std::make_shared<taraxa::FullNode>(context1, 
		std::string("./core_tests/conf_full_node1.json"), 
		std::string("./core_tests/conf_network1.json")));
	
	// node1->setVerbose(true);
	node1->setDebug(true);
	node1->start();
	// send package	
	auto nw2 (std::make_shared<taraxa::Network>(context2, "./core_tests/conf_network2.json"));

	std::unique_ptr<boost::asio::io_context::work> work (new boost::asio::io_context::work(context1));

	boost::thread t([&context1](){
		context1.run();
	});

	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
	nw2->start();
	std::vector<StateBlock> blks;

	StateBlock blk1 (
	"0000000000000000000000000000000000000000000000000000000000000000",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000001",
	"000000000000000000000000000000000000000000000000000000000000000F");

	StateBlock blk2 (
	"0000000000000000000000000000000000000000000000000000000000000001",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000002",
	"000000000000000000000000000000000000000000000000000000000000000F");
	
	StateBlock blk3 (
	"0000000000000000000000000000000000000000000000000000000000000002",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000003",
	"000000000000000000000000000000000000000000000000000000000000000F");

	StateBlock blk4 (
	"0000000000000000000000000000000000000000000000000000000000000003",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000004",
	"000000000000000000000000000000000000000000000000000000000000000F");
	
	StateBlock blk5 (
	"0000000000000000000000000000000000000000000000000000000000000004",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000005",
	"000000000000000000000000000000000000000000000000000000000000000F");
	
	StateBlock blk6 (
	"0000000000000000000000000000000000000000000000000000000000000005",
	{"0000000000000000000000000000000000000000000000000000000000000004", 
	 "0000000000000000000000000000000000000000000000000000000000000003"}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000006",
	"000000000000000000000000000000000000000000000000000000000000000F");

	blks.push_back(blk6);
	blks.push_back(blk5);
	blks.push_back(blk4);
	blks.push_back(blk3);
	blks.push_back(blk2);
	blks.push_back(blk1);
	
	for (auto i=0; i<blks.size(); ++i){
		nw2->sendBlock(ep, blks[i]);
	}

	std::cout<<"Waiting packages for 500 milliseconds ..."<<std::endl;
	taraxa::thisThreadSleepForMilliSeconds(500);

	work.reset();
	nw2->stop();

	std::cout<<"Waiting packages for 500 milliseconds ..."<<std::endl;
	taraxa::thisThreadSleepForMilliSeconds(500);
	node1->stop();

	// node1->drawGraph("dot.txt");
	EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
	EXPECT_EQ(node1->getNumVerticesInDag(), 7);
	EXPECT_EQ(node1->getNumEdgesInDag(), 8);
	EXPECT_EQ(node1->getNumProposedBlocks(),2);
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
// 	std::shared_ptr<Network> nw2 (new taraxa::Network(context2, "./core_tests/conf_network2.json"));

// 	std::unique_ptr<boost::asio::io_context::work> work (new boost::asio::io_context::work(context1));

// 	boost::thread t([&context1](){
// 		context1.run();
// 	});

// 	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
// 	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
// 	nw2->start();
// 	std::vector<StateBlock> blks;

// 	StateBlock blk0 (
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

int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}