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
#include "create_samples.hpp"
#include "transaction.hpp"
namespace taraxa {

const unsigned NUM_TRX = 10;

auto g_trx_samples = samples::createTrxSamples(0, NUM_TRX);

TEST(grpc, server_client) {
  GrpcService gservice;

  std::thread t([&gservice]() {
    gservice.start();
    taraxa::thisThreadSleepForSeconds(1);
  });

  taraxa::thisThreadSleepForSeconds(1);

  Transaction trans1 = g_trx_samples[0];
  Transaction trans2 = g_trx_samples[1];

  GrpcClient client(::grpc::CreateChannel("0.0.0.0:10077",
                                          grpc::InsecureChannelCredentials()));

  client.sendTransaction(g_trx_samples[0]);
  client.sendTransaction(g_trx_samples[1]);
  trx_hash_t hash1 = trans1.getHash();
  trx_hash_t hash2 = trans2.getHash();
  trx_hash_t hash3(
      "2100000000000000000000000000000000000000000000000000000000000001");
  Transaction trans1r = client.getTransaction(hash1);
  Transaction trans2r = client.getTransaction(hash2);
  EXPECT_EQ(trans1, trans1r);
  EXPECT_EQ(trans2, trans2r);
  Transaction trans3r = client.getTransaction(hash3);
  EXPECT_EQ((bool)trans3r.getHash(), false);
  taraxa::thisThreadSleepForSeconds(1);
  gservice.stop();
  t.join();
}

}  // namespace taraxa

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}