/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-18 12:56:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:26:15
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


TEST(FullNode, send_and_receive_three_messages){

	boost::asio::io_context context1;
	boost::asio::io_context context2;

	std::shared_ptr<FullNode> node1 (new taraxa::FullNode(context1, 
		std::string("./core_tests/conf_full_node1.json"), 
		std::string("./core_tests/conf_network1.json"), 
		std::string("./core_tests/conf_rpc1.json")));
	// node1->setVerbose(true);
	node1->setDebug(true);
	node1->start();
	// send package	
	std::shared_ptr<Network> nw2 (new taraxa::Network(context2, "./core_tests/conf_network2.json"));

	std::unique_ptr<boost::asio::io_context::work> work (new boost::asio::io_context::work(context1));

	boost::thread t([&context1](){
		context1.run();
	});

	unsigned port1 = node1->getNetwork()->getConfig().udp_port;
	end_point_udp_t ep(boost::asio::ip::address_v4::loopback(),port1);
	nw2->start();
	std::vector<StateBlock> blks;

	StateBlock blk0 (
	string("1011111111111111111111111111111111111111111111111111111111111111"),
	{
	("2022222222222222222222222222222222222222222222222222222222222222"),
	("3033333333333333333333333333333333333333333333333333333333333333"),
	("4044444444444444444444444444444444444444444444444444444444444444")}, 
	{
	("5055555555555555555555555555555555555555555555555555555555555555"),
	("6066666666666666666666666666666666666666666666666666666666666666")},
	("70777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("8088888888888888888888888888888888888888888888888888888888888888"));
	
	StateBlock blk1 (
	string("1111111111111111111111111111111111111111111111111111111111111111"),
	{
	("2122222222222222222222222222222222222222222222222222222222222222"),
	("3133333333333333333333333333333333333333333333333333333333333333"),
	("4144444444444444444444444444444444444444444444444444444444444444")}, 
	{
	("5155555555555555555555555555555555555555555555555555555555555555"),
	("6166666666666666666666666666666666666666666666666666666666666666")},
	("71777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("8188888888888888888888888888888888888888888888888888888888888888"));

	StateBlock blk2 (
	string("1211111111111111111111111111111111111111111111111111111111111111"),
	{
	("2222222222222222222222222222222222222222222222222222222222222222"),
	("3233333333333333333333333333333333333333333333333333333333333333"),
	("4244444444444444444444444444444444444444444444444444444444444444")}, 
	{
	("5255555555555555555555555555555555555555555555555555555555555555"),
	("6266666666666666666666666666666666666666666666666666666666666666")},
	("72777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("8288888888888888888888888888888888888888888888888888888888888888"));
	
	blks.push_back(blk0);
	blks.push_back(blk1);
	blks.push_back(blk2);
	for (auto i=0; i<blks.size(); ++i){
		nw2->sendBlock(ep, blks[i]);
	}

	std::cout<<"Waiting packages for 1 second ..."<<std::endl;
	taraxa::thisThreadSleepForSeconds(1);

	work.reset();
	nw2->stop();
	node1->stop();

	ASSERT_EQ(node1->getNumReceivedBlocks(), blks.size());
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