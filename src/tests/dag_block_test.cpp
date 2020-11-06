#include "../dag_block.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <vector>

#include "../full_node.hpp"
#include "../static_init.hpp"
#include "../types.hpp"
#include "../util.hpp"
#include "../util_test/samples.hpp"
#include "../util_test/util.hpp"
#include "../vdf_sortition.hpp"

namespace taraxa::core_tests {
const unsigned NUM_TRX = 40;
const unsigned NUM_BLK = 4;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;
using namespace vdf_sortition;

struct DagBlockTest : BaseTest {};

auto g_blk_samples = samples::createMockDagBlkSamples(0, NUM_BLK, 0, BLK_TRX_LEN, BLK_TRX_OVERLAP);

auto g_secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);

TEST_F(DagBlockTest, clear) {
  std::string str("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
  ASSERT_EQ(str.size(), 64);
  uint256_hash_t test(str);
  ASSERT_EQ(test.toString(), "8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
  test.clear();
  ASSERT_EQ(test.toString(), "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST_F(DagBlockTest, serialize_deserialize) {
  vdf_sortition::VdfConfig vdf_config(255, 0, 5, 5, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  Message msg(3);
  VdfSortition vdf(vdf_config, g_key_pair.address(), sk, msg);
  blk_hash_t vdf_input(200);
  vdf.computeVdfSolution(vdf_input.toString());
  DagBlock blk1(blk_hash_t(1), 2, {}, {}, vdf);
  auto b = blk1.rlp(true);
  DagBlock blk2(b);
  EXPECT_EQ(blk1, blk2);
}

TEST_F(DagBlockTest, send_receive_one) {
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

TEST_F(DagBlockTest, send_receive_two) {
  using std::string;
  using std::vector;
  vector<uint256_hash_t> outgoings{blk_hash_t("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
                                   blk_hash_t("7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f")};

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

TEST_F(DagBlockTest, send_receive_three) {
  using std::string;
  using std::vector;
  vector<uint256_hash_t> outgoings{blk_hash_t("8f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
                                   blk_hash_t("7f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
                                   blk_hash_t("6f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f")};

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

TEST_F(DagBlockTest, sender_and_hash_verify) {
  DagBlock blk1(blk_hash_t(111),   // pivot
                0,                 // level
                {blk_hash_t(222),  // tips
                 blk_hash_t(333), blk_hash_t(444)},
                {trx_hash_t(555),  // trxs
                 trx_hash_t(666)});
  blk1.sign(g_secret);
  EXPECT_EQ(g_key_pair.address(), blk1.sender());
  EXPECT_TRUE(blk1.verifySig());

  DagBlock blk_from_rlp(blk1.rlp(true));
  EXPECT_EQ(blk_from_rlp.sender(), blk1.sender());
  EXPECT_EQ(blk_from_rlp.getHash(), blk1.getHash());
}

TEST_F(DagBlockTest, sign_verify) {
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
  EXPECT_EQ(blk1.getSig(), blk1c.getSig()) << blk1 << std::endl << blk1c;
  EXPECT_EQ(blk1.sender(), blk1c.sender());
  EXPECT_EQ(blk1.getHash(), blk1c.getHash());

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

TEST_F(DagBlockTest, push_and_pop) {
  auto node_cfgs = make_node_cfgs(1);
  FullNode::Handle node(node_cfgs[0]);
  BlockManager blk_qu(1024, 2, addr_t(), node->getDB(), nullptr, node->getTimeLogger());
  blk_qu.start();
  DagBlock blk1(blk_hash_t(1111), level_t(0), {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)}, {}, sig_t(7777), blk_hash_t(888), addr_t(999));

  DagBlock blk2(blk_hash_t(21111), level_t(0), {blk_hash_t(2222), blk_hash_t(2333), blk_hash_t(2444)}, {}, sig_t(27777), blk_hash_t(2888),
                addr_t(2999));

  blk_qu.pushUnverifiedBlock(blk1, true);
  blk_qu.pushUnverifiedBlock(blk2, true);

  auto blk3 = blk_qu.popVerifiedBlock();
  auto blk4 = blk_qu.popVerifiedBlock();
  // The order is non-deterministic
  bool res = (blk1 == blk3) ? blk2 == blk4 : blk2 == blk3;
  EXPECT_TRUE(res);
  blk_qu.stop();
}

TEST_F(DagBlockTest, overlap) {
  DagBlock blk1(blk_hash_t(1111), level_t(1), {}, {trx_hash_t(1000), trx_hash_t(2000), trx_hash_t(3000)}, sig_t(7777), blk_hash_t(888), addr_t(999));

  DagBlock blk2(blk_hash_t(1112), level_t(1), {}, {trx_hash_t(100), trx_hash_t(2000), trx_hash_t(3000), trx_hash_t(1000)}, sig_t(7777),
                blk_hash_t(888), addr_t(999));

  addr_t addr;
  TransactionOrderManager detector(addr, nullptr);
  TransactionExecStatusTable table;
  auto overlap1 = detector.computeOrderInBlock(blk1, table);
  auto overlap2 = detector.computeOrderInBlock(blk2, table);
  EXPECT_TRUE(overlap1[0]);
  EXPECT_TRUE(overlap1[1]);
  EXPECT_TRUE(overlap1[2]);

  EXPECT_TRUE(overlap2[0]);
  EXPECT_FALSE(overlap2[1]);
  EXPECT_FALSE(overlap2[2]);
  EXPECT_FALSE(overlap2[3]);
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  static_init();
  LoggingConfig logging;
  addr_t node_addr;
  logging.verbosity = taraxa::VerbosityError;
  setupLoggingConfiguration(node_addr, logging);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}