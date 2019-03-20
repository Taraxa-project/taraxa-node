/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-02-27 15:39:04
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 16:57:09
 */

#include "transaction.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "grpc_server.hpp"

namespace taraxa {
TEST(transaction, serialize_deserialize) {
  Transaction trans1(
      "1000000000000000000000000000000000000000000000000000000000000001",  // hash
      Transaction::Type::Null,  // type
      "2000000000000000000000000000000000000000000000000000000000000002",  // nonce
      "3000000000000000000000000000000000000000000000000000000000000003",  // value
      "4000000000000000000000000000000000000000000000000000000000000004",  // gas_price
      "5000000000000000000000000000000000000000000000000000000000000005",  // gas
      "6000000000000000000000000000000000000000000000000000000000000006",  // receiver
      "777777777777777777777777777777777777777777777777777777777777777777777777"
      "77777777777777777777777777777777777777777777777777777777",  // sig
      str2bytes("00FEDCBA9876543210000000"));
  std::stringstream ss1, ss2;
  ss1 << trans1;
  std::vector<uint8_t> bytes;
  {
    vectorstream strm1(bytes);
    trans1.serialize(strm1);
  }

  bufferstream strm2(bytes.data(), bytes.size());

  Transaction trans2(strm2);
  ss2 << trans2;
  // Compare transaction content
  ASSERT_EQ(ss1.str(), ss2.str());
  // Compare json format
  ASSERT_EQ(trans1.getJsonStr(), trans2.getJsonStr());
  // Get data size, check leading and training zeros
  ASSERT_EQ(trans2.getData().size(), 12);
  // Construct transaction from json
  Transaction trans3(trans2.getJsonStr());
  ASSERT_EQ(trans2.getJsonStr(), trans3.getJsonStr());
}

}  // namespace taraxa

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
