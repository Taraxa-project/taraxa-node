#include "dag/dag.hpp"
#include "gtest.hpp"
#include "pbft/block_proposer.hpp"
#include "pbft/pbft_manager.hpp"
#include "samples.hpp"

namespace taraxa::core_tests {

struct NodeDagCreationFixture : BaseTest {
  uint64_t nonce = 0;
  uint64_t dummy_nonce = 0;
  std::shared_ptr<FullNode> node;
  dev::KeyPair dummy = dev::KeyPair::create();
  std::optional<addr_t> contract_addr;

  NodeDagCreationFixture() : BaseTest() {}
  ~NodeDagCreationFixture() = default;
  struct DagBlockWithTxs {
    DagBlock blk;
    SharedTransactions trxs;
  };
  void modifyConfig(FullNodeConfig &cfg) {
    auto &vdf_config = cfg.chain.sortition.vdf;
    vdf_config.difficulty_min = 1;
    vdf_config.difficulty_max = 3;
    vdf_config.difficulty_stale = 4;
    cfg.chain.pbft.ghost_path_move_back = 0;
  }
  void makeNode(bool start = true) {
    auto cfgs = make_node_cfgs<5, true>(1);
    modifyConfig(cfgs.front());
    node = create_nodes(cfgs, start).front();
  }
  void makeNodeFromConfig(std::vector<FullNodeConfig> cfgs, bool start = true) {
    modifyConfig(cfgs.front());
    node = create_nodes(cfgs, start).front();

    auto trx = std::make_shared<Transaction>(0, 1000000, 0, 0, bytes(), node->getSecretKey(), dummy.address());
    auto [ok, _] = node->getTransactionManager()->insertTransaction(trx);
    ASSERT_TRUE(ok);
    nonce++;
  }

  uint32_t getInitialDagSize() { return node->getConfig().max_levels_per_period; }

  void dummyTransaction() {
    auto trx = std::make_shared<Transaction>(dummy_nonce, 1, 0, 0, bytes(), dummy.secret(), node->getAddress());
    auto [ok, m] = node->getTransactionManager()->insertTransaction(trx);
    ASSERT_TRUE(ok);
    dummy_nonce++;
  }

  void deployContract() {
    auto trx = std::make_shared<Transaction>(nonce, 100, 0, 0, dev::fromHex(samples::greeter_contract_code),
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

  uint64_t trxEstimation() {
    const auto &transactions = makeTransactions(1);
    static auto estimation = node->getTransactionManager()->estimateTransactionGas(transactions.front(), {});

    return estimation;
  }

  SharedTransactions makeTransactions(uint32_t count) {
    SharedTransactions result;
    auto _nonce = nonce;
    for (auto i = _nonce; i < _nonce + count; ++i) {
      result.emplace_back(
          std::make_shared<Transaction>(i, 11, 0, 0,
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

  void insertBlocks(std::vector<DagBlockWithTxs> blks_with_txs) {
    for (auto &b : blks_with_txs) {
      for (auto t : b.trxs) {
        node->getTransactionManager()->insertTransaction(t);
      }
      node->getDagManager()->addDagBlock(std::move(b.blk), std::move(b.trxs));
    }
  }

  void insertTransactions(SharedTransactions transactions) {
    for (const auto &trx : transactions) {
      auto insert_result = node->getTransactionManager()->insertTransaction(trx);
      EXPECT_EQ(insert_result.first, true);
    }
  }

  void generateAndApplyInitialDag() { insertBlocks(generateDagBlocks(getInitialDagSize(), 1, 1)); }

  std::vector<DagBlockWithTxs> generateDagBlocks(uint16_t levels, uint16_t blocks_per_level, uint16_t trx_per_block) {
    std::vector<DagBlockWithTxs> result;
    auto start_level = node->getDagManager()->getMaxLevel() + 1;
    auto &db = node->getDB();
    auto dag_genesis = node->getConfig().chain.dag_genesis_block.getHash();
    SortitionConfig vdf_config(node->getConfig().chain.sortition);

    auto transactions = makeTransactions(levels * blocks_per_level * trx_per_block);
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
        DagBlock blk(pivot, level, tips, trx_hashes, std::vector<uint64_t>(trx_per_block, trx_estimation), vdf,
                     node->getSecretKey());
        this_level_blocks.push_back(blk.getHash());
        result.emplace_back(DagBlockWithTxs{blk, SharedTransactions(trx_itr, trx_itr_next)});
        trx_itr = trx_itr_next;
      }
      tips = this_level_blocks;
      pivot = this_level_blocks.front();
    }

    // create more dag blocks to finalize all previous
    const auto proposal_period = db->getProposalPeriodForDagLevel(start_level + levels);
    const auto period_block_hash = db->getPeriodBlockHash(*proposal_period);
    for (auto i = 0; i < 1; ++i) {
      auto level = start_level + levels + i;
      vdf_sortition::VdfSortition vdf(vdf_config, node->getVrfSecretKey(),
                                      vrf_wrapper::VrfSortitionBase::makeVrfInput(level, period_block_hash));
      vdf.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
      DagBlock blk(pivot, level + i, tips, {transactions.rbegin()->get()->getHash()},
                   std::vector<uint64_t>(trx_per_block, trx_estimation), vdf, node->getSecretKey());
      result.emplace_back(DagBlockWithTxs{blk, SharedTransactions(transactions.rbegin(), transactions.rbegin() + 1)});
      pivot = blk.getHash();
      tips = {blk.getHash()};
    }

    EXPECT_EQ(trx_itr_next, transactions.end());

    return result;
  }
};

}  // namespace taraxa::core_tests