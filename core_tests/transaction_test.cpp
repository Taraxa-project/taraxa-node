#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "static_init.hpp"
#include "util/eth.hpp"
#include "util/lazy.hpp"

namespace taraxa {
using ::taraxa::util::lazy::Lazy;

const unsigned NUM_TRX = 40;
const unsigned NUM_BLK = 4;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;
auto g_secret = Lazy([] {
  return dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_trx_samples =
    Lazy([] { return samples::createMockTrxSamples(0, NUM_TRX); });
auto g_signed_trx_samples =
    Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });
auto g_blk_samples = Lazy([] {
  return samples::createMockDagBlkSamples(0, NUM_BLK, 0, BLK_TRX_LEN,
                                          BLK_TRX_OVERLAP);
});

struct TransactionTest : core_tests::util::DBUsingTest<> {};

TEST_F(TransactionTest, serialize_deserialize) {
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

TEST_F(TransactionTest, signer_signature_verify) {
  Transaction trans1 = g_trx_samples[0];
  Transaction trans2 = g_trx_samples[1];
  auto pk = dev::toPublic(g_secret);
  trans1.sign(g_secret);
  trans2.sign(g_secret);
  EXPECT_NE(trans1.getSig(), trans2.getSig());
  EXPECT_NE(trans1.getHash(), trans2.getHash());
  EXPECT_EQ(trans1.sender(), trans2.sender());
  EXPECT_TRUE(trans1.verifySig());
  EXPECT_TRUE(trans2.verifySig());
}

TEST_F(TransactionTest, verifiers) {
  TransactionStatusTable status_table(100000, 1000);
  AccountNonceTable accs_table;

  TransactionQueue trx_qu(status_table, accs_table, 2 /*num verifiers*/);
  trx_qu.setVerifyMode(TransactionQueue::VerifyMode::skip_verify_sig);
  trx_qu.start();

  // insert trx
  std::thread t([&trx_qu]() {
    for (auto const& t : *g_trx_samples) {
      trx_qu.insert(t, true);
    }
  });

  // insert trx again, should not duplicated
  for (auto const& t : *g_trx_samples) {
    trx_qu.insert(t, false);
  }
  t.join();
  thisThreadSleepForMilliSeconds(100);
  auto verified_trxs = trx_qu.moveVerifiedTrxSnapShot();
  EXPECT_EQ(verified_trxs.size(), g_trx_samples->size());
}

TEST_F(TransactionTest, prepare_signed_trx_for_propose) {
  TransactionStatusTable status_table(100000, 1000);
  TransactionManager trx_mgr(
      std::make_shared<DbStorage>("/tmp/rocksdb/test", blk_hash_t(), true));
  trx_mgr.start();

  std::thread insertTrx([&trx_mgr]() {
    for (auto const& t : *g_signed_trx_samples) {
      trx_mgr.insertTrx(t, t.rlp(true), false);
    }
  });

  thisThreadSleepForMilliSeconds(500);

  insertTrx.join();
  vec_trx_t total_packed_trxs, packed_trxs;
  std::cout << "Start block proposing ..." << std::endl;
  std::thread wakeup([&trx_mgr]() {
    thisThreadSleepForSeconds(2);
    trx_mgr.stop();
  });
  do {
    DagFrontier frontier;
    trx_mgr.packTrxs(packed_trxs, frontier);
    total_packed_trxs.insert(total_packed_trxs.end(), packed_trxs.begin(),
                             packed_trxs.end());
    thisThreadSleepForMicroSeconds(100);
  } while (!packed_trxs.empty());
  wakeup.join();
  EXPECT_EQ(total_packed_trxs.size(), NUM_TRX)
      << " Packed Trx: " << ::testing::PrintToString(total_packed_trxs);
}

}  // namespace taraxa

int main(int argc, char* argv[]) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
