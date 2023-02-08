#include "dag/dag_block.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <vector>

#include "common/static_init.hpp"
#include "common/types.hpp"
#include "common/util.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_proposer.hpp"
#include "dag/dag_manager.hpp"
#include "logger/logger.hpp"
#include "node/node.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"
#include "vdf/sortition.hpp"

namespace taraxa::core_tests {
const unsigned NUM_BLK = 4;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;
using namespace vdf_sortition;

struct DagBlockTest : NodesTest {};
struct DagBlockMgrTest : NodesTest {};

auto g_blk_samples = samples::createMockDagBlkSamples(0, NUM_BLK, 0, BLK_TRX_LEN, BLK_TRX_OVERLAP);

auto g_secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                            dev::Secret::ConstructFromStringType::FromHex);
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
  SortitionParams sortition_params(0xFFFF, 0, 5, 5, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 3;
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level), 1, 100);
  blk_hash_t vdf_input(200);
  vdf.computeVdfSolution(sortition_params, vdf_input.asBytes(), false);
  DagBlock blk1(blk_hash_t(1), 2, {}, {}, {}, vdf, secret_t::random());
  auto b = blk1.rlp(true);
  DagBlock blk2(b);
  EXPECT_EQ(blk1, blk2);

  DagBlock blk3(blk1.getJsonStr());
  EXPECT_EQ(blk1, blk3);
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
                 trx_hash_t(666)},
                g_secret);
  EXPECT_EQ(g_key_pair.address(), blk1.getSender());
  EXPECT_TRUE(blk1.verifySig());

  DagBlock blk_from_rlp(blk1.rlp(true));
  EXPECT_EQ(blk_from_rlp.getSender(), blk1.getSender());
  EXPECT_EQ(blk_from_rlp.getHash(), blk1.getHash());
}

TEST_F(DagBlockTest, sign_verify) {
  DagBlock blk1(blk_hash_t(111),   // pivot
                0,                 // level
                {blk_hash_t(222),  // tips
                 blk_hash_t(333), blk_hash_t(444)},
                {trx_hash_t(555),  // trxs
                 trx_hash_t(666)},
                g_secret);
  DagBlock blk1c(blk_hash_t(111),   // pivot
                 0,                 // level
                 {blk_hash_t(222),  // tips
                  blk_hash_t(333), blk_hash_t(444)},
                 {trx_hash_t(555),  // trxs
                  trx_hash_t(666)},
                 g_secret);
  EXPECT_EQ(blk1.getSig(), blk1c.getSig()) << blk1 << std::endl << blk1c;
  EXPECT_EQ(blk1.getSender(), blk1c.getSender());
  EXPECT_EQ(blk1.getHash(), blk1c.getHash());

  EXPECT_TRUE(blk1.verifySig());

  DagBlock blk2(blk_hash_t(9999),  // pivot
                0,                 // level
                {},                // tips,
                {}, g_secret);     // trxs

  EXPECT_NE(blk1.getSig(), blk2.getSig());
  EXPECT_NE(blk1.getHash(), blk2.getHash());
  EXPECT_EQ(blk2.getSender(), blk1.getSender());

  EXPECT_TRUE(blk2.verifySig());
}

TEST_F(DagBlockMgrTest, proposal_period) {
  auto node = create_nodes(1).front();
  auto db = node->getDB();
  auto dag_mgr = node->getDagManager();

  // Proposal period 0 has in DB already at DAG block manager constructor
  auto proposal_period = db->getProposalPeriodForDagLevel(10);
  EXPECT_TRUE(proposal_period);
  EXPECT_EQ(*proposal_period, 0);
  proposal_period = db->getProposalPeriodForDagLevel(100);
  EXPECT_TRUE(proposal_period);
  EXPECT_EQ(*proposal_period, 0);

  db->saveProposalPeriodDagLevelsMap(110, *proposal_period + 1);
  proposal_period = db->getProposalPeriodForDagLevel(101);
  EXPECT_TRUE(proposal_period);
  EXPECT_EQ(*proposal_period, 1);
  proposal_period = db->getProposalPeriodForDagLevel(110);
  EXPECT_TRUE(proposal_period);
  EXPECT_EQ(*proposal_period, 1);

  db->saveProposalPeriodDagLevelsMap(130, *proposal_period + 1);
  proposal_period = db->getProposalPeriodForDagLevel(111);
  EXPECT_TRUE(proposal_period);
  EXPECT_EQ(*proposal_period, 2);
  proposal_period = db->getProposalPeriodForDagLevel(130);
  EXPECT_TRUE(proposal_period);
  EXPECT_EQ(*proposal_period, 2);

  // Proposal period not exsit
  proposal_period = db->getProposalPeriodForDagLevel(131);
  EXPECT_FALSE(proposal_period);
}

TEST_F(DagBlockMgrTest, incorrect_tx_estimation) {
  auto node = create_nodes(1).front();
  auto db = node->getDB();
  auto dag_mgr = node->getDagManager();

  auto trx = samples::createSignedTrxSamples(1, 1, g_secret).front();
  auto insert_result = node->getTransactionManager()->insertTransaction(trx);
  ASSERT_EQ(insert_result.second, "");

  // Generate DAG blocks
  auto dag_genesis = node->getConfig().genesis.dag_genesis_block.getHash();
  SortitionConfig vdf_config(node->getConfig().genesis.sortition);
  auto propose_level = 1;
  const auto period_block_hash = node->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, node->getVrfSecretKey(),
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);

  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trx});
  vdf1.computeVdfSolution(vdf_config, vdf_msg, false);

  // transactions.size and estimations size is not equal
  {
    DagBlock blk(dag_genesis, propose_level, {}, {trx->getHash()}, {}, vdf1, node->getSecretKey());
    EXPECT_EQ(node->getDagManager()->verifyBlock(std::move(blk)),
              DagManager::VerifyBlockReturnType::IncorrectTransactionsEstimation);
  }

  // wrong estimated tx
  {
    DagBlock blk(dag_genesis, propose_level, {}, {trx->getHash()}, 100, vdf1, node->getSecretKey());
    EXPECT_EQ(node->getDagManager()->verifyBlock(std::move(blk)),
              DagManager::VerifyBlockReturnType::IncorrectTransactionsEstimation);
  }
}

TEST_F(DagBlockMgrTest, dag_block_tips_verification) {
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  auto node = create_nodes(node_cfgs).front();

  auto trxs = samples::createSignedTrxSamples(1, kDagBlockMaxTips + 1, g_secret);
  for (auto trx : trxs) {
    auto insert_result = node->getTransactionManager()->insertTransaction(trx);
    ASSERT_EQ(insert_result.second, "");
  }
  // Generate DAG blocks
  auto dag_genesis = node->getConfig().genesis.dag_genesis_block.getHash();
  SortitionConfig vdf_config(node->getConfig().genesis.sortition);
  auto propose_level = 1;
  const auto period_block_hash = node->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf(vdf_config, node->getVrfSecretKey(),
                                  VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);

  std::vector<blk_hash_t> dag_blocks_hashes;
  for (auto trx : trxs) {
    dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trx});
    vdf.computeVdfSolution(vdf_config, vdf_msg, false);

    DagBlock blk(dag_genesis, propose_level, {}, {trx->getHash()}, 100000, vdf, node->getSecretKey());
    dag_blocks_hashes.push_back(blk.getHash());
    EXPECT_EQ(node->getDagManager()->verifyBlock(blk), DagManager::VerifyBlockReturnType::Verified);
    EXPECT_TRUE(node->getDagManager()->addDagBlock(std::move(blk), {trx}).first);
  }

  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trxs[0]});
  vdf.computeVdfSolution(vdf_config, vdf_msg, false);

  // Verify block over the kDagBlockMaxTips is rejected
  DagBlock blk_over_limit(dag_genesis, propose_level, dag_blocks_hashes, {trxs[0]->getHash()}, 100000, vdf,
                          node->getSecretKey());
  EXPECT_EQ(node->getDagManager()->verifyBlock(blk_over_limit),
            DagManager::VerifyBlockReturnType::FailedTipsVerification);

  // Verify block at kDagBlockMaxTips is accepted
  dag_blocks_hashes.resize(kDagBlockMaxTips);
  DagBlock blk_at_limit(dag_genesis, propose_level, dag_blocks_hashes, {trxs[0]->getHash()}, 100000, vdf,
                        node->getSecretKey());
  EXPECT_EQ(node->getDagManager()->verifyBlock(blk_at_limit), DagManager::VerifyBlockReturnType::Verified);

  // Verify block below kDagBlockMaxTips is accepted
  dag_blocks_hashes.resize(kDagBlockMaxTips - 1);
  DagBlock blk_under_limit(dag_genesis, propose_level, dag_blocks_hashes, {trxs[0]->getHash()}, 100000, vdf,
                           node->getSecretKey());
  EXPECT_EQ(node->getDagManager()->verifyBlock(blk_under_limit), DagManager::VerifyBlockReturnType::Verified);

  auto dag_blocks_hashes_with_duplicate_pivot = dag_blocks_hashes;
  dag_blocks_hashes_with_duplicate_pivot.push_back(dag_genesis);

  auto dag_blocks_hashes_with_duplicate_tip = dag_blocks_hashes;
  dag_blocks_hashes_with_duplicate_tip.push_back(dag_blocks_hashes[0]);

  // Verify block with duplicate pivot is rejected
  DagBlock blk_with_duplicate_pivot(dag_genesis, propose_level, dag_blocks_hashes_with_duplicate_pivot,
                                    {trxs[0]->getHash()}, 100000, vdf, node->getSecretKey());
  EXPECT_EQ(node->getDagManager()->verifyBlock(blk_with_duplicate_pivot),
            DagManager::VerifyBlockReturnType::FailedTipsVerification);

  // Verify block with duplicate tip is rejected
  DagBlock blk_with_duplicate_tip(dag_genesis, propose_level, dag_blocks_hashes_with_duplicate_tip,
                                  {trxs[0]->getHash()}, 100000, vdf, node->getSecretKey());
  EXPECT_EQ(node->getDagManager()->verifyBlock(blk_with_duplicate_tip),
            DagManager::VerifyBlockReturnType::FailedTipsVerification);
}

TEST_F(DagBlockMgrTest, dag_block_tips_proposal) {
  auto node_cfgs = make_node_cfgs(2, 1, 20);
  auto node = create_nodes(node_cfgs).front();

  auto trxs = samples::createSignedTrxSamples(1, kDagBlockMaxTips + 1, g_secret);
  for (auto trx : trxs) {
    auto insert_result = node->getTransactionManager()->insertTransaction(trx);
    ASSERT_EQ(insert_result.second, "");
  }
  // Generate DAG blocks
  auto dag_genesis = node->getConfig().genesis.dag_genesis_block.getHash();
  SortitionConfig vdf_config(node->getConfig().genesis.sortition);
  auto propose_level = 1;
  auto period_block_hash = node->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf(vdf_config, node->getVrfSecretKey(),
                                  VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);

  std::vector<blk_hash_t> dag_blocks_hashes;
  for (auto trx : trxs) {
    dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trx});
    vdf.computeVdfSolution(vdf_config, vdf_msg, false);

    DagBlock blk(dag_genesis, propose_level, {}, {trx->getHash()}, 100000, vdf, node->getSecretKey());
    dag_blocks_hashes.push_back(blk.getHash());
    EXPECT_EQ(node->getDagManager()->verifyBlock(blk), DagManager::VerifyBlockReturnType::Verified);
    EXPECT_TRUE(node->getDagManager()->addDagBlock(std::move(blk), {trx}).first);
  }

  // Verify selection is up to kDagBlockMaxTips
  EXPECT_EQ(dag_blocks_hashes.size(), kDagBlockMaxTips + 1);
  auto selected_tips = node->getDagBlockProposer()->selectDagBlockTips(dag_blocks_hashes);
  EXPECT_EQ(selected_tips.size(), kDagBlockMaxTips);

  // Verify selection is up to kDagBlockMaxTips and higher propose level has priority
  propose_level = 2;
  period_block_hash = node->getDB()->getPeriodBlockHash(propose_level);
  vdf = vdf_sortition::VdfSortition(vdf_config, node->getVrfSecretKey(),
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);
  for (auto trx : trxs) {
    dev::bytes vdf_msg = DagManager::getVdfMessage(dag_blocks_hashes[0], {trx});
    vdf.computeVdfSolution(vdf_config, vdf_msg, false);

    DagBlock blk(dag_blocks_hashes[0], propose_level, {}, {trx->getHash()}, 100000, vdf, node->getSecretKey());
    dag_blocks_hashes.push_back(blk.getHash());
    EXPECT_EQ(node->getDagManager()->verifyBlock(blk), DagManager::VerifyBlockReturnType::Verified);
    EXPECT_TRUE(node->getDagManager()->addDagBlock(std::move(blk), {trx}).first);
  }

  selected_tips = node->getDagBlockProposer()->selectDagBlockTips(dag_blocks_hashes);
  EXPECT_EQ(selected_tips.size(), kDagBlockMaxTips);
  for (auto t : selected_tips) {
    EXPECT_EQ(node->getDagManager()->getDagBlock(t)->getLevel(), propose_level);
  }

  // Verify selection is up to kDagBlockMaxTips and unique proposer has priority
  propose_level = 1;
  period_block_hash = node->getDB()->getPeriodBlockHash(propose_level);
  vdf = vdf_sortition::VdfSortition(vdf_config, node_cfgs[1].vrf_secret,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);

  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trxs[0]});
  vdf.computeVdfSolution(vdf_config, vdf_msg, false);

  DagBlock blk(dag_genesis, propose_level, {}, {trxs[0]->getHash()}, 100000, vdf, node_cfgs[1].node_secret);
  dag_blocks_hashes.push_back(blk.getHash());
  EXPECT_TRUE(node->getDagManager()->addDagBlock(std::move(blk), {trxs[0]}).first);

  selected_tips = node->getDagBlockProposer()->selectDagBlockTips(dag_blocks_hashes);
  EXPECT_EQ(selected_tips.size(), kDagBlockMaxTips);
  std::unordered_map<uint32_t, uint32_t> level_count;
  for (auto t : selected_tips) {
    level_count[node->getDagManager()->getDagBlock(t)->getLevel()]++;
  }
  EXPECT_EQ(level_count.size(), 2);
  EXPECT_EQ(level_count[1], 1);
  EXPECT_EQ(level_count[2], kDagBlockMaxTips - 1);
}

TEST_F(DagBlockMgrTest, too_big_dag_block) {
  // make config
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  node_cfgs.front().genesis.dag.gas_limit = 500000;

  auto node = create_nodes(node_cfgs).front();
  auto db = node->getDB();

  std::vector<trx_hash_t> hashes;
  uint64_t estimations = 0;
  const size_t count = 5;
  for (size_t i = 0; i <= count; ++i) {
    auto create_trx = std::make_shared<Transaction>(i + 1, 100, 0, 200001, dev::fromHex(samples::greeter_contract_code),
                                                    node->getSecretKey());
    auto [ok, err_msg] = node->getTransactionManager()->insertTransaction(create_trx);
    EXPECT_EQ(ok, true);
    hashes.emplace_back(create_trx->getHash());
    const auto& e = node->getTransactionManager()->estimateTransactionGas(create_trx, std::nullopt);
    estimations += e;
  }

  // Generate DAG block
  auto dag_genesis = node->getConfig().genesis.dag_genesis_block.getHash();
  SortitionConfig vdf_config(node->getConfig().genesis.sortition);
  auto propose_level = 1;
  const auto period_block_hash = node->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, node->getVrfSecretKey(),
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);
  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, hashes);
  vdf1.computeVdfSolution(vdf_config, vdf_msg, false);

  {
    DagBlock blk(dag_genesis, propose_level, {}, hashes, estimations, vdf1, node->getSecretKey());
    EXPECT_EQ(node->getDagManager()->verifyBlock(std::move(blk)), DagManager::VerifyBlockReturnType::BlockTooBig);
  }
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
