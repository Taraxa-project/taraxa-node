/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-28 11:12:22
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 21:24:34
 */

#include "libdevcore/Log.h"
#include "network.hpp"
#include <atomic>
#include <boost/thread.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

namespace taraxa {

TEST(Network, transfer_block) {
    std::shared_ptr<Network> nw1(
	new taraxa::Network("./core_tests/conf_network1.json"));
    std::shared_ptr<Network> nw2(
	new taraxa::Network("./core_tests/conf_network2.json"));

    nw1->start();
    nw2->start();

    DagBlock blk(
	"1111111111111111111111111111111111111111111111111111111111111111",
	{"2222222222222222222222222222222222222222222222222222222222222222",
	 "3333333333333333333333333333333333333333333333333333333333333333",
	 "4444444444444444444444444444444444444444444444444444444444444444"},
	{"5555555555555555555555555555555555555555555555555555555555555555",
	 "6666666666666666666666666666666666666666666666666666666666666666"},
	"7777777777777777777777777777777777777777777777777777777777777777777777"
	"7777777777777777777777777777777777777777777777777777777777",
	"8888888888888888888888888888888888888888888888888888888888888888",
	"000000000000000000000000000000000000000000000000000000000000000F");

    taraxa::thisThreadSleepForSeconds(10);

    for (auto i = 0; i < 1; ++i) {
	nw2->sendBlock(nw1->getNodeId(), blk);
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


} // namespace taraxa

int main(int argc, char **argv) {
    dev::LoggingOptions logOptions;
    logOptions.verbosity = dev::VerbositySilent;
    dev::setupLogging(logOptions);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}