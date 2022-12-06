#include "test_util/node_dag_creation_fixture.hpp"

namespace taraxa::core_tests {

void NodeDagCreationFixture::modifyConfig(FullNodeConfig &cfg) {
  auto &vdf_config = cfg.genesis.sortition.vdf;
  vdf_config.difficulty_min = 1;
  vdf_config.difficulty_max = 3;
  vdf_config.difficulty_stale = 4;
  cfg.genesis.pbft.ghost_path_move_back = 0;
}
void NodeDagCreationFixture::makeNode(bool start) {
  auto cfgs = make_node_cfgs(1, 1, 5, true);
  modifyConfig(cfgs.front());
  node = create_nodes(cfgs, start).front();
}
void NodeDagCreationFixture::makeNodeFromConfig(std::vector<FullNodeConfig> cfgs, bool start) {
  modifyConfig(cfgs.front());
  node = create_nodes(cfgs, start).front();

  auto trx = std::make_shared<Transaction>(nonce, 1000000, 0, TEST_TX_GAS_LIMIT, bytes(), node->getSecretKey(),
                                           dummy.address());
  auto [ok, _] = node->getTransactionManager()->insertTransaction(trx);
  ASSERT_TRUE(ok);
  nonce++;
}

uint32_t NodeDagCreationFixture::getInitialDagSize() { return node->getConfig().max_levels_per_period; }

void NodeDagCreationFixture::dummyTransaction() {
  auto trx =
      std::make_shared<Transaction>(dummy_nonce, 1, 0, TEST_TX_GAS_LIMIT, bytes(), dummy.secret(), node->getAddress());
  auto [ok, m] = node->getTransactionManager()->insertTransaction(trx);
  ASSERT_TRUE(ok);
  dummy_nonce++;
}

void NodeDagCreationFixture::deployContract() {
  auto trx = std::make_shared<Transaction>(nonce, 0, 0, TEST_TX_GAS_LIMIT, dev::fromHex(samples::greeter_contract_code),
                                           node->getSecretKey());
  auto [ok, err_msg] = node->getTransactionManager()->insertTransaction(trx);
  ASSERT_TRUE(ok);
  nonce++;

  EXPECT_HAPPENS({30s, 1s}, [&](auto &ctx) {
    WAIT_EXPECT_TRUE(ctx, node->getDB()->transactionFinalized(trx->getHash()));

    if (!contract_addr) {
      auto receipt = node->getFinalChain()->transaction_receipt(trx->getHash());
      WAIT_EXPECT_TRUE(ctx, receipt.has_value());
      WAIT_EXPECT_TRUE(ctx, receipt->new_contract_address.has_value());
      contract_addr = receipt->new_contract_address;
    }
    auto r = node->getFinalChain()->transaction_receipt(trx->getHash());

    WAIT_EXPECT_TRUE(ctx, !node->getFinalChain()->get_code(contract_addr.value()).empty());
  });
  ASSERT_TRUE(contract_addr.has_value());
  std::cout << "Contract deployed: " << contract_addr.value() << std::endl;
}

uint64_t NodeDagCreationFixture::trxEstimation() {
  const auto &transactions = makeTransactions(1);
  static auto estimation = node->getTransactionManager()->estimateTransactionGas(transactions.front(), {});
  assert(estimation);
  return estimation;
}

SharedTransactions NodeDagCreationFixture::makeTransactions(uint32_t count) {
  SharedTransactions result;
  auto _nonce = nonce;
  for (auto i = _nonce; i < _nonce + count; ++i) {
    result.emplace_back(
        std::make_shared<Transaction>(i, 11, 0, TEST_TX_GAS_LIMIT,
                                      // setGreeting("Hola")
                                      dev::fromHex("0xa4136862000000000000000000000000000000000000000000000000"
                                                   "00000000000000200000000000000000000000000000000000000000000"
                                                   "000000000000000000004486f6c61000000000000000000000000000000"
                                                   "00000000000000000000000000"),
                                      node->getSecretKey(), contract_addr));
  }
  nonce += count;
  return result;
}

void NodeDagCreationFixture::insertBlocks(std::vector<DagBlockWithTxs> blks_with_txs) {
  for (auto &b : blks_with_txs) {
    for (auto t : b.trxs) {
      node->getTransactionManager()->insertTransaction(t);
    }
    node->getDagManager()->addDagBlock(std::move(b.blk), std::move(b.trxs));
  }
}

void NodeDagCreationFixture::insertTransactions(SharedTransactions transactions) {
  for (const auto &trx : transactions) {
    auto insert_result = node->getTransactionManager()->insertTransaction(trx);
    EXPECT_EQ(insert_result.first, true);
  }
}

void NodeDagCreationFixture::generateAndApplyInitialDag() {
  insertBlocks(generateDagBlocks(getInitialDagSize(), 1, 1));
}

std::vector<NodeDagCreationFixture::DagBlockWithTxs> NodeDagCreationFixture::generateDagBlocks(
    uint16_t levels, uint16_t blocks_per_level, uint16_t trx_per_block) {
  std::vector<DagBlockWithTxs> result;
  auto start_level = node->getDagManager()->getMaxLevel() + 1;
  auto &db = node->getDB();
  auto dag_genesis = node->getConfig().genesis.dag_genesis_block.getHash();
  SortitionConfig vdf_config(node->getConfig().genesis.sortition);

  auto transactions = makeTransactions(levels * blocks_per_level * trx_per_block + 1);
  auto trx_estimation = node->getTransactionManager()->estimateTransactionGas(transactions.front(), {});

  blk_hash_t pivot = dag_genesis;
  vec_blk_t tips;

  auto pivot_and_tips = node->getDagManager()->getLatestPivotAndTips();
  if (pivot_and_tips) {
    pivot = pivot_and_tips->first;
    tips = pivot_and_tips->second;
  }

  auto trx_itr = transactions.begin();
  auto trx_itr_next = transactions.begin();

  for (size_t level = start_level; level < start_level + levels; ++level) {
    // save hashes of all dag blocks from this level to use as tips for next level blocks
    vec_blk_t this_level_blocks;
    for (size_t block_n = 0; block_n < blocks_per_level; ++block_n) {
      trx_itr_next += trx_per_block;
      const auto proposal_period = db->getProposalPeriodForDagLevel(level);
      const auto period_block_hash = db->getPeriodBlockHash(*proposal_period);
      vdf_sortition::VdfSortition vdf(vdf_config, node->getVrfSecretKey(),
                                      vrf_wrapper::VrfSortitionBase::makeVrfInput(level, period_block_hash));
      vdf.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
      std::vector<trx_hash_t> trx_hashes;
      std::transform(trx_itr, trx_itr_next, std::back_inserter(trx_hashes),
                     [](std::shared_ptr<Transaction> trx) { return trx->getHash(); });
      DagBlock blk(pivot, level, tips, trx_hashes, trx_per_block * trx_estimation, vdf, node->getSecretKey());
      this_level_blocks.push_back(blk.getHash());
      result.emplace_back(DagBlockWithTxs{blk, SharedTransactions(trx_itr, trx_itr_next)});
      trx_itr = trx_itr_next;
    }
    tips = this_level_blocks;
    pivot = this_level_blocks.front();
  }

  // create one more dag block to finalize all previous
  const auto proposal_period = db->getProposalPeriodForDagLevel(start_level + levels);
  const auto period_block_hash = db->getPeriodBlockHash(*proposal_period);
  auto level = start_level + levels;
  vdf_sortition::VdfSortition vdf(vdf_config, node->getVrfSecretKey(),
                                  vrf_wrapper::VrfSortitionBase::makeVrfInput(level, period_block_hash));
  vdf.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk(pivot, level, tips, {transactions.rbegin()->get()->getHash()}, trx_per_block * trx_estimation, vdf,
               node->getSecretKey());
  result.emplace_back(DagBlockWithTxs{blk, SharedTransactions(transactions.rbegin(), transactions.rbegin() + 1)});
  pivot = blk.getHash();
  tips = {blk.getHash()};

  trx_itr_next++;
  EXPECT_EQ(trx_itr_next, transactions.end());

  return result;
}

}  // namespace taraxa::core_tests