/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-05 16:55:07
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 18:20:34
 */
#include <gtest/gtest.h>
#include <grpc_client.hpp>
#include <grpc_server.hpp>
#include <grpc_util.hpp>
#include <thread>
#include "transaction.hpp"

namespace taraxa {
TEST(grpc, server_client) {
  GrpcService gservice;

  std::thread t([&gservice]() {
    gservice.start();
    taraxa::thisThreadSleepForSeconds(1);
  });

  taraxa::thisThreadSleepForSeconds(1);

  Transaction trans1(
      "1000000000000000000000000000000000000000000000000000000000000001",  // hash
      Transaction::Type::Null,  // type
      2,                        // nonce
      3,                        // value
      "4000000000000000000000000000000000000000000000000000000000000004",  // gas_price
      "5000000000000000000000000000000000000000000000000000000000000005",  // gas
      "6000000000000000000000000000000000000000000000000000000000000006",  // receiver
      "777777777777777777777777777777777777777777777777777777777777777777777777"
      "77777777777777777777777777777777777777777777777777777777",  // sig
      str2bytes("00FEDCBA9876543210000000"));

  Transaction trans2(
      "1100000000000000000000000000000000000000000000000000000000000001",  // hash
      Transaction::Type::Null,  // type
      22,                       // nonce
      33,                       // value
      "4100000000000000000000000000000000000000000000000000000000000004",  // gas_price
      "5100000000000000000000000000000000000000000000000000000000000005",  // gas
      "6100000000000000000000000000000000000000000000000000000000000006",  // receiver
      "777777777777777777777777777777777777777777777777777777777777777777777777"
      "77777777777777777777777777777777777777777777777777777777",  // sig
      str2bytes("00001100FEDCBA9876543210000000"));
  GrpcClient client(::grpc::CreateChannel("0.0.0.0:10077",
                                          grpc::InsecureChannelCredentials()));

  client.sendTransaction(trans1);
  client.sendTransaction(trans2);
  trx_hash_t hash1(
      "1000000000000000000000000000000000000000000000000000000000000001");
  trx_hash_t hash2(
      "1100000000000000000000000000000000000000000000000000000000000001");
  trx_hash_t hash3(
      "2100000000000000000000000000000000000000000000000000000000000001");
  Transaction trans1r = client.getTransaction(hash1);
  Transaction trans2r = client.getTransaction(hash2);
  EXPECT_EQ(trans1, trans1r);
  EXPECT_EQ(trans2, trans2r);
  Transaction trans3r = client.getTransaction(hash3);
  EXPECT_EQ(trans3r.isValid(), false);
  taraxa::thisThreadSleepForSeconds(1);
  gservice.stop();
  t.join();
}

}  // namespace taraxa

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}