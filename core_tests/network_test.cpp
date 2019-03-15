/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-28 11:12:22 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 21:24:34
 */
 
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <vector>
#include <atomic>
#include <iostream>
#include "network.hpp"
#include "libdevcore/Log.h"

namespace taraxa {

TEST(Network, transfer_block){
	std::shared_ptr<Network> nw1 (new taraxa::Network("./core_tests/conf_network1.json"));
	std::shared_ptr<Network> nw2 (new taraxa::Network("./core_tests/conf_network2.json"));
	
	nw1->start();
	nw2->start();
	
	StateBlock blk (
	"1111111111111111111111111111111111111111111111111111111111111111",
	{
	"2222222222222222222222222222222222222222222222222222222222222222",
	"3333333333333333333333333333333333333333333333333333333333333333",
	"4444444444444444444444444444444444444444444444444444444444444444"}, 
	{
	"5555555555555555555555555555555555555555555555555555555555555555",
	"6666666666666666666666666666666666666666666666666666666666666666"},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"8888888888888888888888888888888888888888888888888888888888888888",
	"000000000000000000000000000000000000000000000000000000000000000F");

	taraxa::thisThreadSleepForSeconds(10);

	for (auto i=0; i<1; ++i){
		nw2->sendBlock(nw1->getNodeId(), blk);
	}
	
	std::cout<<"Waiting packages for 10 seconds ..."<<std::endl;

	taraxa::thisThreadSleepForSeconds(10);

	nw2->stop();
	nw1->stop();
	unsigned long long num_received = nw1->getReceivedBlocksCount();
	ASSERT_EQ(1, num_received);

}
}  // namespace taraxa

int main(int argc, char** argv){
	dev::LoggingOptions logOptions;
	logOptions.verbosity = dev::VerbositySilent;
	dev::setupLogging(logOptions);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}