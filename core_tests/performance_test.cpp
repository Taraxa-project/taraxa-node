
#include <gperftools/profiler.h>
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "dag.hpp"
#include "full_node.hpp"
#include <libdevcore/DBFactory.h>
#include <libdevcore/Log.h>
#include "net/RpcServer.h"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "pbft_manager.hpp"
#include "string"
#include "top.hpp"

namespace taraxa {
using namespace core_tests::util;
using samples::sendTrx;
const unsigned NUM_TRX = 50000;

struct PerformanceTest : core_tests::util::DBUsingTest<> {};

TEST_F(PerformanceTest, execute_transactions) {
  val_t initbal(1000000000);  // disable pbft sortition
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  // //
  addr_t acc1 = addr(cfg.node_secret);
  cfg.genesis_state.accounts[acc1] = {initbal};

  auto transactions =
      samples::createSignedTrxSamples(0, NUM_TRX, dev::Secret(cfg.node_secret));

  boost::asio::io_context context;
  auto node(taraxa::FullNode::make(context, cfg));

  node->start(true);  // boot node
  auto res = node->getBalance(acc1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, initbal);

  for (auto const &t : transactions) {
    node->insertTransaction(t);
    // taraxa::thisThreadSleepForMilliSeconds(50);
  }

  taraxa::thisThreadSleepForMilliSeconds(5000);

  EXPECT_GT(node->getNumProposedBlocks(), 0);

  // The test will form a single chain
  std::vector<std::string> ghost;
  node->getGhostPath(cfg.genesis_state.block.getHash().toString(), ghost);
  vec_blk_t blks;
  std::vector<std::vector<uint>> modes;
  ASSERT_GT(ghost.size(), 1);
  uint64_t period = 0, cur_period, cur_period2;
  std::shared_ptr<vec_blk_t> order;
  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();
  // create a period for every 2 pivots, skip genesis
  ProfilerStart("my_prof.txt");
  for (int i = 1; i < ghost.size(); i += 2) {
    auto anchor = blk_hash_t(ghost[i]);
    std::tie(cur_period, order) = node->getDagBlockOrder(anchor);
    // call twice should not change states
    std::tie(cur_period2, order) = node->getDagBlockOrder(anchor);
    EXPECT_EQ(cur_period, cur_period2);
    EXPECT_EQ(cur_period, ++period);
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
        trx_overlap_table = node->computeTransactionOverlapTable(order);
    EXPECT_NE(trx_overlap_table, nullptr);
    std::vector<std::vector<uint>> blocks_trx_modes =
        node->createMockTrxSchedule(trx_overlap_table);
    TrxSchedule sche(*order, blocks_trx_modes);
    ScheduleBlock sche_blk(blk_hash_t(100), sche);
    // set period
    node->setDagBlockOrder(anchor, cur_period);
    bool ret = node->executeScheduleBlock(
        sche_blk, pbft_mgr->sortition_account_balance_table);
    EXPECT_TRUE(ret);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }
  // pickup the last period when dag (chain) size is odd number
  if (ghost.size() % 2 == 1) {
    std::tie(cur_period, order) =
        node->getDagBlockOrder(blk_hash_t(ghost.back()));
    EXPECT_EQ(cur_period, ++period);
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
        trx_overlap_table = node->computeTransactionOverlapTable(order);
    EXPECT_NE(trx_overlap_table, nullptr);
    std::vector<std::vector<uint>> blocks_trx_modes =
        node->createMockTrxSchedule(trx_overlap_table);
    TrxSchedule sche(*order, blocks_trx_modes);
    ScheduleBlock sche_blk(blk_hash_t(100), sche);
    bool ret = node->executeScheduleBlock(
        sche_blk, pbft_mgr->sortition_account_balance_table);
    EXPECT_TRUE(ret);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }
  ProfilerStop();
  node->stop();
  // TODO because of the nonce rule, testing distributing coins
  // from single account requires more thought
  //  EXPECT_EQ(state->balance(acc1), initbal - coin_distributed);
}
}  // namespace taraxa

int main(int argc, char **argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  // logOptions.includeChannels.push_back("FULLND");
  // logOptions.includeChannels.push_back("DAGMGR");
  // logOptions.includeChannels.push_back("EXETOR");
  // logOptions.includeChannels.push_back("BLK_PP");
  // logOptions.includeChannels.push_back("PR_MDL");

  dev::setupLogging(logOptions);
  dev::db::setDatabaseKind(dev::db::DatabaseKind::RocksDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
