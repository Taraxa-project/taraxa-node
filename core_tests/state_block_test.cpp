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

TEST (uint256_hash_t, clear){
	std::string str("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
	ASSERT_EQ(str.size(), 64);
	uint256_hash_t test(str);
	ASSERT_EQ(test.toString(), "8F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F");
	test.clear();
	ASSERT_EQ(test.toString(), "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST (uint256_hash_t, send_receive_one){
	uint256_hash_t send("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
	
	std::vector<uint8_t> data;
	{
		vectorstream strm(data);
		write(strm, send);
	}

	ASSERT_EQ(data.size(), 32);
	bufferstream strm2(data.data(), data.size());	
	uint256_hash_t recv;
	read(strm2, recv);

	ASSERT_EQ(send, recv);
}

TEST (uint256_hash_t, send_receive_two_string){
	using std::vector;
	using std::string;
	vector<uint256_hash_t> outgoings {
		string("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
		string("7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f")};
	
	vector<uint8_t> data;
	{
		vectorstream strm(data);
		for (auto const & i: outgoings){
			write (strm, i);
		}
	}
	ASSERT_EQ(data.size(), 32*2);

	vector<uint256_hash_t> receivings(2);
	bufferstream strm2(data.data(), data.size());
	for (auto i = 0; i < 2; ++i){
		uint256_hash_t & recv = receivings[i];
		read(strm2, recv);
	}
	ASSERT_EQ(outgoings, receivings);
}

TEST (uint256_hash_t, send_receive_two_cstr){
	using std::vector;
	using std::string;
	vector<uint256_hash_t> outgoings {
		"8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f",
		"7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"};
	
	vector<uint8_t> data;
	{
		vectorstream strm(data);
		for (auto const & i: outgoings){
			write (strm, i);
		}
	}
	ASSERT_EQ(data.size(), 32*2);

	vector<uint256_hash_t> receivings(2);
	bufferstream strm2(data.data(), data.size());
	for (auto i = 0; i < 2; ++i){
		uint256_hash_t & recv = receivings[i];
		read(strm2, recv);
	}
	ASSERT_EQ(outgoings, receivings);
}

TEST (uint256_hash_t, send_receive_three_string){
	using std::vector;
	using std::string;
	vector<uint256_hash_t> outgoings {
		string("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
		string("7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
		string("6f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f")};
	
	vector<uint8_t> data;
	{
		vectorstream strm(data);
		for (auto const & i: outgoings){
			write (strm, i);
		}
	}
	ASSERT_EQ(data.size(), 32*3);

	vector<uint256_hash_t> receivings(3);
	bufferstream strm2(data.data(), data.size());
	for (auto i = 0; i < 3; ++i){
		uint256_hash_t & recv = receivings[i];
		read(strm2, recv);
	}
	ASSERT_EQ(outgoings, receivings);
}

TEST (uint256_hash_t, send_receive_three_cstr){
	using std::vector;
	using std::string;
	vector<uint256_hash_t> outgoings {
		"8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f",
		"7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f",
		"6f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"};
	
	vector<uint8_t> data;
	{
		vectorstream strm(data);
		for (auto const & i: outgoings){
			write (strm, i);
		}
	}
	ASSERT_EQ(data.size(), 32*3);

	vector<uint256_hash_t> receivings(3);
	bufferstream strm2(data.data(), data.size());
	for (auto i = 0; i < 3; ++i){
		uint256_hash_t & recv = receivings[i];
		read(strm2, recv);
	}
	ASSERT_EQ(outgoings, receivings);
}

TEST (StateBlock, string_format){
	using std::string; 
	StateBlock blk (
	string("1111111111111111111111111111111111111111111111111111111111111111"),
	{
	("2222222222222222222222222222222222222222222222222222222222222222"),
	("3333333333333333333333333333333333333333333333333333333333333333"),
	("4444444444444444444444444444444444444444444444444444444444444444")}, 
	{
	("5555555555555555555555555555555555555555555555555555555555555555"),
	("6666666666666666666666666666666666666666666666666666666666666666")},
	//"7F26AFD8ADED36D94DFEAF2BB6BA82CBCA0935684D1B00B4A45DC01F9A1652B964DB68C02601E1EE061E3A12E18F8DB4EF1C378CA2C1ACC63D00A3D715195CCD",
	("7777777777777777777777777777777777777777777777777777777777777777"),
	("8888888888888888888888888888888888888888888888888888888888888888"));
	std::string json = blk.getJsonStr();
	std::stringstream ss1, ss2;
	ss1<<blk;
	std::vector<uint8_t> bytes;
	// Need to put a scope of vectorstream, other bytes won't get result.
	{
		vectorstream strm1(bytes);
		blk.serialize(strm1);
	}
	// check stream size
	ASSERT_EQ(bytes.size(), 258);
	bufferstream strm2 (bytes.data(), bytes.size());
	StateBlock blk2;
	blk2.deserialize(strm2);
	ss2<<blk2;
	// Compare block content
	ASSERT_EQ(ss1.str(), ss2.str());
	// Compare json output
	ASSERT_EQ(blk.getJsonStr(), blk2.getJsonStr());
}

}
int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}