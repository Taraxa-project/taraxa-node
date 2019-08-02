/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-09
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-07-26
 */
#include "pbft_manager.hpp"

#include "full_node.hpp"
#include "libdevcore/DBFactory.h"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "network.hpp"
#include "top.hpp"

#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include "core_tests/util.hpp"

namespace taraxa {
using namespace core_tests::util;

struct PbftManagerTest : public DBUsingTest<> {};
struct PbftVoteTest : public DBUsingTest<> {};
struct NetworkTest : public DBUsingTest<> {};
struct VoteManagerTest : public DBUsingTest<> {};

TEST_F(PbftManagerTest, pbft_manager_lambda_input_test) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  uint lambda_ms = 1000;
  uint committee_size = 3;
  uint valid_sortition_coins = 10000;
  std::vector<uint> pbft_params{lambda_ms, committee_size,
                                valid_sortition_coins};

  PbftManager pbft_manager(pbft_params, GENESIS);
  EXPECT_EQ(lambda_ms, pbft_manager.LAMBDA_ms);
  EXPECT_EQ(committee_size, pbft_manager.COMMITTEE_SIZE);
  EXPECT_EQ(valid_sortition_coins, pbft_manager.VALID_SORTITION_COINS);
}

TEST_F(PbftManagerTest, full_node_lambda_input_test) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
  auto pbft_mgr = node->getPbftManager();
  EXPECT_EQ(pbft_mgr->LAMBDA_ms, 1000);
  EXPECT_EQ(pbft_mgr->COMMITTEE_SIZE, 3);
  EXPECT_EQ(pbft_mgr->VALID_SORTITION_COINS, 10000);
}

/* Place votes period 1, 2 and 3 into vote queue.
 * Get vote period 2, will remove period 1 in the queue. Queue size changes
 * to 2.
 */
TEST_F(PbftVoteTest, DISABLED_pbft_place_and_get_vote_test) {
  const char* input1[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json", "-v", "0"};

  Top top1(5, input1);
  EXPECT_TRUE(top1.isActive());
  thisThreadSleepForMilliSeconds(500);

  auto node = top1.getNode();

  node->clearUnverifiedVotesTable();

  try {
    system("./core_tests/scripts/curl_pbft_place_vote.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  try {
    system("./core_tests/scripts/curl_pbft_get_votes.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  node->stop();

  size_t votes_size = node->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 2);
}

// Add votes round 1, 2 and 3 into unverified vote table
// Get votes round 2, will remove round 1 in the table, and return round 2 & 3
// votes
TEST_F(VoteManagerTest, add_cleanup_get_votes) {
  const char* input[] = {"./build/main", "--conf_taraxa",
                         "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  Top top(5, input);
  EXPECT_TRUE(top.isActive());
  auto node = top.getNode();
  EXPECT_NE(node, nullptr);

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  std::shared_ptr<VoteManager> vote_mgr = node->getVoteManager();
  node->clearUnverifiedVotesTable();

  // generate 6 votes, each round has 2 votes
  for (int i = 1; i <= 3; i++) {
    for (int j = 1; j <= 2; j++) {
      blk_hash_t blockhash(1);
      PbftVoteTypes type = propose_vote_type;
      uint64_t round = i;
      size_t step = j;
      Vote vote = node->generateVote(blockhash, type, round, step);
      node->addVote(vote);
    }
  }
  // Test add vote
  size_t votes_size = node->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 6);

  // Test get votes
  std::vector<Vote> votes = node->getVotes(2);
  EXPECT_EQ(votes.size(), 4);
  for (Vote const& v : votes) {
    EXPECT_GT(v.getRound(), 1);
  }

  // Test cleanup votes
  votes_size = node->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 4);
  vote_mgr->cleanupVotes(4);  // cleanup round 2 & 3
  votes_size = node->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 0);

  top.kill();
}

// Generate a vote, send the vote from node2 to node1
TEST_F(NetworkTest, transfer_vote) {
  // set nodes account balance
  val_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  vector<FullNodeConfig> cfgs;
  for (auto i = 1; i <= 2; ++i) {
    cfgs.emplace_back(fmt("./core_tests/conf/conf_taraxa%s.json", i));
  }
  for (auto& cfg : cfgs) {
    for (auto& cfg_other : cfgs) {
      cfg.genesis_state.accounts[addr(cfg_other.node_secret)] = {new_balance};
    }
  }
  auto node_count = 0;
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(context1, cfgs[node_count++]));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(context2, cfgs[node_count++]));

  node1->setDebug(true);
  node2->setDebug(true);
  node1->start(true);  // boot node
  node2->start(false);

  std::unique_ptr<boost::asio::io_context::work> work1(
      new boost::asio::io_context::work(context1));
  std::unique_ptr<boost::asio::io_context::work> work2(
      new boost::asio::io_context::work(context2));

  boost::thread t1([&context1]() { context1.run(); });
  boost::thread t2([&context2]() { context2.run(); });

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();

  int node_peers = 1;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers &&
        nw2->getPeerCount() == node_peers) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_EQ(node_peers, nw1->getPeerCount());
  ASSERT_EQ(node_peers, nw2->getPeerCount());

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  pbft_mgr1->stop();
  pbft_mgr2->stop();

  // generate vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;
  Vote vote = node2->generateVote(blockhash, type, period, step);

  node1->clearUnverifiedVotesTable();
  node2->clearUnverifiedVotesTable();

  nw2->sendPbftVote(nw1->getNodeId(), vote);

  work1.reset();
  work2.reset();
  node1->stop();
  node2->stop();
  t1.join();
  t2.join();

  size_t vote_queue_size = node1->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size, 1);
}

TEST_F(NetworkTest, vote_broadcast) {
  // set nodes account balance
  val_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  vector<FullNodeConfig> cfgs;
  for (auto i = 1; i <= 3; ++i) {
    cfgs.emplace_back(fmt("./core_tests/conf/conf_taraxa%s.json", i));
  }
  for (auto& cfg : cfgs) {
    for (auto& cfg_other : cfgs) {
      cfg.genesis_state.accounts[addr(cfg_other.node_secret)] = {new_balance};
    }
  }
  auto node_count = 0;
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(context1, cfgs[node_count++]));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(context2, cfgs[node_count++]));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(context3, cfgs[node_count++]));
  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true);  // boot node
  node2->start(false);
  node3->start(false);

  std::unique_ptr<boost::asio::io_context::work> work1(
      new boost::asio::io_context::work(context1));
  std::unique_ptr<boost::asio::io_context::work> work2(
      new boost::asio::io_context::work(context2));
  std::unique_ptr<boost::asio::io_context::work> work3(
      new boost::asio::io_context::work(context3));

  boost::thread t1([&context1]() { context1.run(); });
  boost::thread t2([&context2]() { context2.run(); });
  boost::thread t3([&context3]() { context3.run(); });

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();

  int node_peers = 2;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers &&
        nw2->getPeerCount() == node_peers &&
        nw3->getPeerCount() == node_peers) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_EQ(node_peers, nw1->getPeerCount());
  ASSERT_EQ(node_peers, nw2->getPeerCount());
  ASSERT_EQ(node_peers, nw3->getPeerCount());

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();
  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

  // generate vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;
  Vote vote = node1->generateVote(blockhash, type, period, step);

  node1->clearUnverifiedVotesTable();
  node2->clearUnverifiedVotesTable();
  node3->clearUnverifiedVotesTable();

  nw1->onNewPbftVote(vote);

  work1.reset();
  work2.reset();
  work3.reset();
  node1->stop();
  node2->stop();
  node3->stop();
  t1.join();
  t2.join();
  t3.join();

  size_t vote_queue_size1 = node1->getUnverifiedVotesSize();
  size_t vote_queue_size2 = node2->getUnverifiedVotesSize();
  size_t vote_queue_size3 = node3->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size1, 0);
  EXPECT_EQ(vote_queue_size2, 1);
  EXPECT_EQ(vote_queue_size3, 1);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  logOptions.includeChannels.push_back("NETWORK");
  logOptions.includeChannels.push_back("TARCAP");
  logOptions.includeChannels.push_back("VOTE_MGR");
  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
