/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-02-27 15:39:04
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 19:15:44
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include "create_samples.hpp"
#include "grpc_server.hpp"
namespace taraxa {
const unsigned NUM_TRX = 40;
const unsigned NUM_BLK = 4;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;

auto g_trx_samples = samples::createTrxSamples(0, NUM_TRX);
auto g_blk_samples =
    samples::createDagBlkSamples(0, NUM_BLK, 0, BLK_TRX_LEN, BLK_TRX_OVERLAP);
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);

TEST(transaction, serialize_deserialize) {
  Transaction& trans1 = g_trx_samples[0];
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

TEST(Transaction, signer) {
  Transaction trans1 = g_trx_samples[0];
  Transaction trans2 = g_trx_samples[1];

  std::cout << "sig: " << trans1.getSig() << std::endl;
  std::cout << "sender: " << trans1.getSender() << std::endl;
  trans1.sign(g_secret);
  trans2.sign(g_secret);
  std::cout << trans1 << std::endl;
  std::cout << trans2 << std::endl;

  std::cout << "sig: " << trans1.getSig() << std::endl;
  std::cout << "sender: " << trans1.getSender() << std::endl;
  std::cout << "sig: " << trans2.getSig() << std::endl;
  std::cout << "sender: " << trans2.getSender() << std::endl;
}

TEST(TransactionQueue, verifiers) {
  TransactionStatusTable status_table;
  TransactionQueue trx_qu(status_table, 2 /*num verifiers*/);
  trx_qu.start();

  // insert trx
  std::thread t([&trx_qu]() {
    for (auto const& t : g_trx_samples) {
      trx_qu.insert(t);
    }
  });

  // insert trx again, should not duplicated
  for (auto const& t : g_trx_samples) {
    trx_qu.insert(t);
  }
  t.join();
  thisThreadSleepForMilliSeconds(100);
  auto verified_trxs = trx_qu.moveVerifiedTrxSnapShot();
  EXPECT_EQ(verified_trxs.size(), g_trx_samples.size());
}

TEST(TransactionManager, prepare_trx_for_propose) {
  TransactionStatusTable status_table;
  auto db_block = std::make_shared<RocksDb>("/tmp/rocksdb/blk");
  TransactionManager trx_mgr(db_block);
  std::thread insertTrx([&trx_mgr]() {
    for (auto const& t : g_trx_samples) {
      trx_mgr.insertTrx(t);
    }
  });
  std::thread insertBlk([&trx_mgr]() {
    for (auto const& b : g_blk_samples) {
      trx_mgr.setPackedTrxFromBlock(b);
    }
  });
  thisThreadSleepForSeconds(1);
  std::cout << "First batch of insertions ... done." << std::endl;

  insertTrx.join();
  insertBlk.join();
  vec_trx_t to_be_packed_trxs;
  unsigned packed_trx = 0;

  // trying to insert same trans when proposing
  std::thread insertTrx2([&trx_mgr]() {
    for (auto const& t : g_trx_samples) {
      trx_mgr.insertTrx(t);
    }
  });
  std::cout << "Start block proposing ..." << std::endl;
  do {
    trx_mgr.packTrxs(to_be_packed_trxs);
    packed_trx += to_be_packed_trxs.size();
    thisThreadSleepForMicroSeconds(100);
  } while (!to_be_packed_trxs.empty());

  // trying to insert same trans when proposing
  std::thread insertTrx3([&trx_mgr]() {
    for (auto const& t : g_trx_samples) {
      trx_mgr.insertTrx(t);
    }
  });
  insertTrx2.join();
  insertTrx3.join();
  EXPECT_EQ(packed_trx,
            NUM_TRX - NUM_BLK * (BLK_TRX_LEN - BLK_TRX_OVERLAP) - 1);
}

}  // namespace taraxa

int main(int argc, char* argv[]) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
