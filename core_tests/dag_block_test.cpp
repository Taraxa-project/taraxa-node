/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-14 12:41:38
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 21:24:27
 */

#include "dag_block.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "types.hpp"

namespace taraxa {
const unsigned NUM_TRX = 40;
const unsigned NUM_BLK = 4;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;

auto g_blk_samples = samples::createMockDagBlkSamples(
    0, NUM_BLK, 0, BLK_TRX_LEN, BLK_TRX_OVERLAP);

auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);

TEST(uint256_hash_t, clear) {
  std::string str(
      "8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
  ASSERT_EQ(str.size(), 64);
  uint256_hash_t test(str);
  ASSERT_EQ(test.toString(),
            "8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
  test.clear();
  ASSERT_EQ(test.toString(),
            "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST(uint256_hash_t, send_receive_one) {
  uint256_hash_t send(
      "8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");

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

TEST(uint256_hash_t, send_receive_two) {
  using std::string;
  using std::vector;
  vector<uint256_hash_t> outgoings{
      blk_hash_t(
          "8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
      blk_hash_t(
          "7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f")};

  vector<uint8_t> data;
  {
    vectorstream strm(data);
    for (auto const& i : outgoings) {
      write(strm, i);
    }
  }
  ASSERT_EQ(data.size(), 32 * 2);

  vector<uint256_hash_t> receivings(2);
  bufferstream strm2(data.data(), data.size());
  for (auto i = 0; i < 2; ++i) {
    uint256_hash_t& recv = receivings[i];
    read(strm2, recv);
  }
  ASSERT_EQ(outgoings, receivings);
}

TEST(uint256_hash_t, send_receive_three) {
  using std::string;
  using std::vector;
  vector<uint256_hash_t> outgoings{
      blk_hash_t(
          "8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
      blk_hash_t(
          "7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
      blk_hash_t(
          "6f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f")};

  vector<uint8_t> data;
  {
    vectorstream strm(data);
    for (auto const& i : outgoings) {
      write(strm, i);
    }
  }
  ASSERT_EQ(data.size(), 32 * 3);

  vector<uint256_hash_t> receivings(3);
  bufferstream strm2(data.data(), data.size());
  for (auto i = 0; i < 3; ++i) {
    uint256_hash_t& recv = receivings[i];
    read(strm2, recv);
  }
  ASSERT_EQ(outgoings, receivings);
}

TEST(DagBlock, string_format) {
  using std::string;
  DagBlock blk(
      blk_hash_t(
          "1111111111111111111111111111111111111111111111111111111111111111"),
      level_t(0),
      {blk_hash_t(
           "2222222222222222222222222222222222222222222222222222222222222222"),
       blk_hash_t(
           "3333333333333333333333333333333333333333333333333333333333333333"),
       blk_hash_t(
           "4444444444444444444444444444444444444444444444444444444444444444")},
      {trx_hash_t(
           "5555555555555555555555555555555555555555555555555555555555555555"),
       trx_hash_t(
           "6666666666666666666666666666666666666666666666666666666666666666")},
      sig_t("777777777777777777777777777777777777777777777777777777777777777777"
            "777777"
            "777777777777777777777777777777777777777777777777777777777"),
      blk_hash_t(
          "8888888888888888888888888888888888888888888888888888888888888888"),
      addr_t("0000000000000000000000000000000000055555"));
  std::string json = blk.getJsonStr();
  std::stringstream ss1, ss2;
  ss1 << blk;
  std::vector<uint8_t> bytes;
  // Need to put a scope of vectorstream, other bytes won't get result.
  {
    vectorstream strm1(bytes);
    blk.serialize(strm1);
  }
  // check stream size
  ASSERT_EQ(bytes.size(), 325);
  bufferstream strm2(bytes.data(), bytes.size());
  DagBlock blk2;
  blk2.deserialize(strm2);
  ss2 << blk2;
  // Compare block content
  ASSERT_EQ(ss1.str(), ss2.str());
  // Compare json output
  ASSERT_EQ(blk.getJsonStr(), blk2.getJsonStr());
}

TEST(DagBlock, sign_verify) {
  DagBlock blk1(blk_hash_t(111),   // pivot
                0,                 // level
                {blk_hash_t(222),  // tips
                 blk_hash_t(333), blk_hash_t(444)},
                {trx_hash_t(555),  // trxs
                 trx_hash_t(666)});
  DagBlock blk1c(blk_hash_t(111),   // pivot
                 0,                 // level
                 {blk_hash_t(222),  // tips
                  blk_hash_t(333), blk_hash_t(444)},
                 {trx_hash_t(555),  // trxs
                  trx_hash_t(666)});
  blk1.sign(g_secret);
  blk1c.sign(g_secret);
  EXPECT_EQ(blk1.getSig(), blk1c.getSig());
  EXPECT_EQ(blk1.sender(), blk1.sender());
  EXPECT_EQ(blk1.getHash(), blk1.getHash());

  EXPECT_TRUE(blk1.verifySig());

  DagBlock blk2(blk_hash_t(9999),  // pivot
                0,                 // level
                {},                // tips,
                {});               // trxs
  blk2.sign(g_secret);

  EXPECT_NE(blk1.getSig(), blk2.getSig());
  EXPECT_NE(blk1.getHash(), blk2.getHash());
  EXPECT_EQ(blk2.sender(), blk1.sender());

  EXPECT_TRUE(blk2.verifySig());
}

TEST(BlockManager, push_and_pop) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
  BlockManager blk_qu(1024, 2);
  blk_qu.setFullNode(node->getShared());
  blk_qu.start();
  DagBlock blk1(blk_hash_t(1111), level_t(0),
                {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)}, {},
                sig_t(7777), blk_hash_t(888), addr_t(999));

  DagBlock blk2(blk_hash_t(21111), level_t(0),
                {blk_hash_t(2222), blk_hash_t(2333), blk_hash_t(2444)}, {},
                sig_t(27777), blk_hash_t(2888), addr_t(2999));

  blk_qu.pushUnverifiedBlock(blk1, true);
  blk_qu.pushUnverifiedBlock(blk2, true);

  auto blk3 = blk_qu.popVerifiedBlock();
  auto blk4 = blk_qu.popVerifiedBlock();
  // The order is non-deterministic
  bool res = (blk1 == blk3) ? blk2 == blk4 : blk2 == blk3;
  EXPECT_TRUE(res);
  blk_qu.stop();
}

TEST(TransactionOrderManager, overlap) {
  DagBlock blk1(blk_hash_t(1111), level_t(1), {},
                {trx_hash_t(1000), trx_hash_t(2000), trx_hash_t(3000)},
                sig_t(7777), blk_hash_t(888), addr_t(999));

  DagBlock blk2(
      blk_hash_t(1112), level_t(1), {},
      {trx_hash_t(100), trx_hash_t(2000), trx_hash_t(3000), trx_hash_t(1000)},
      sig_t(7777), blk_hash_t(888), addr_t(999));

  TransactionOrderManager detector;
  auto overlap1 = detector.computeOrderInBlock(blk1);
  auto overlap2 = detector.computeOrderInBlock(blk2);
  EXPECT_TRUE(overlap1[0]);
  EXPECT_TRUE(overlap1[1]);
  EXPECT_TRUE(overlap1[2]);

  EXPECT_TRUE(overlap2[0]);
  EXPECT_FALSE(overlap2[1]);
  EXPECT_FALSE(overlap2[2]);
  EXPECT_FALSE(overlap2[3]);
}

}  // namespace taraxa
int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}