/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-10 11:38:44 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-10 12:07:56
 */
 
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <vector>
#include <atomic>
#include <iostream>
#include "network.hpp"
#include "full_node.hpp"

namespace taraxa {
	TEST(Network, udp_receive){
	std::string conf_full_node1 = "conf_full_node1.json";
	std::string conf_full_node2 = "conf_full_node2.json";

	std::shared_ptr<FullNode> node1 (new taraxa::FullNode(conf_full_node1));
	std::shared_ptr<FullNode> node2 (new taraxa::FullNode(conf_full_node2));


	node1->setVerbose(true);
	node2->setVerbose(true);

	std::cout<<"Full node 1 is set"<<std::endl;
	std::cout<<"Full node 2 is set"<<std::endl;

	
}

}  // namespace taraxa

int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}