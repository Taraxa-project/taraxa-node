/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-14 12:41:38 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-14 12:46:53
 */
 
#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include "types.hpp"
#include "state_block.hpp"

namespace taraxa{

TEST (StateBlock, json_format){
	StateBlock blk ("1",{"A","B"}, {"C","D"},"ABCD","2000");
	std::string json = blk.getJsonStr();
	std::cout<<json<<std::endl;
}

}
int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}