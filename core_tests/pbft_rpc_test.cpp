#include <gtest/gtest.h>
#include <libdevcore/DBFactory.h>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>

#include <boost/thread.hpp>

#include "core_tests/util.hpp"
#include "full_node.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "static_init.hpp"
#include "top.hpp"
#include "vote.h"

namespace taraxa {
using namespace core_tests::util;
using namespace vrf_wrapper;
auto g_vrf_sk = Lazy([] {
  return vrf_sk_t(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
});
auto g_sk = Lazy([] {
  return secret_t(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
struct PbftRpcTest : core_tests::util::DBUsingTest<> {};

TEST_F(PbftRpcTest, pbft_manager_lambda_input_test) {
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

TEST_F(PbftRpcTest, full_node_lambda_input_test) {
  auto node(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json")));
  auto pbft_mgr = node->getPbftManager();
  EXPECT_EQ(pbft_mgr->LAMBDA_ms, 2000);
  EXPECT_EQ(pbft_mgr->VALID_SORTITION_COINS, 1000000000);
}

// Add votes round 1, 2 and 3 into unverified vote table
// Get votes round 2, will remove round 1 in the table, and return round 2 & 3
// votes
TEST_F(PbftRpcTest, add_cleanup_get_votes) {
  const char* input[] = {"./build/main", "--conf_taraxa",
                         "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  Top top(5, input);
  auto node = top.getNode();

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  std::shared_ptr<VoteManager> vote_mgr = node->getVoteManager();
  node->clearUnverifiedVotesTable();

  // generate 6 votes, each round has 2 votes
  for (int i = 1; i <= 3; i++) {
    for (int j = 1; j <= 2; j++) {
      blk_hash_t blockhash(1);
      blk_hash_t pbft_blockhash =
          pbft_mgr->getLastPbftBlockHashAtStartOfRound();
      PbftVoteTypes type = propose_vote_type;
      uint64_t round = i;
      size_t step = j;
      Vote vote =
          node->generateVote(blockhash, type, round, step, pbft_blockhash);
      node->addVote(vote);
    }
  }
  // Test add vote
  size_t votes_size = node->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 6);

  // Test get votes
  // CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
  size_t valid_sortition_players = 1;
  pbft_mgr->setPbftThreshold(valid_sortition_players);
  uint64_t pbft_round = 2;
  std::vector<Vote> votes =
      vote_mgr->getVotes(pbft_round, valid_sortition_players);
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
}

TEST_F(PbftRpcTest, reconstruct_votes) {
  public_t pk(12345);
  sig_t sortition_sig(1234567);
  sig_t vote_sig(9878766);
  blk_hash_t blk_hash(111111);
  PbftVoteTypes type(propose_vote_type);
  uint64_t round(999);
  size_t step(2);
  VrfSortition vrf_sortition(g_vrf_sk, blk_hash_t(123), type, round, step);
  Vote vote1(g_sk, vrf_sortition, blk_hash);
  auto rlp = vote1.rlp();
  Vote vote2(rlp);
  EXPECT_EQ(vote1, vote2);
}

// Generate a vote, send the vote from node2 to node1
TEST_F(PbftRpcTest, transfer_vote) {
  // set nodes account balance
  val_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  vector<FullNodeConfig> cfgs;
  for (auto i = 1; i <= 2; ++i) {
    cfgs.emplace_back(fmt("./core_tests/conf/conf_taraxa%s.json", i));
  }
  for (auto& cfg : cfgs) {
    for (auto& cfg_other : cfgs) {
      cfg.eth_chain_params.genesisState[addr(cfg_other.node_secret)] =
          dev::eth::Account(0, new_balance);
    }
    cfg.eth_chain_params.calculateStateRoot(true);
  }
  auto node_count = 0;
  auto node1(taraxa::FullNode::make(cfgs[node_count++]));
  auto node2(taraxa::FullNode::make(cfgs[node_count++]));

  node1->setDebug(true);
  node2->setDebug(true);
  node1->start(true);  // boot node
  node2->start(false);

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

  // generate vote
  blk_hash_t blockhash(11);
  blk_hash_t pbft_blockhash(991);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;

  Vote vote =
      node2->generateVote(blockhash, type, period, step, pbft_blockhash);

  node1->clearUnverifiedVotesTable();
  node2->clearUnverifiedVotesTable();

  nw2->sendPbftVote(nw1->getNodeId(), vote);

  // fixme stopping before asserts
  pbft_mgr1->stop();
  pbft_mgr2->stop();

  size_t vote_queue_size_in_node1 = node1->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size_in_node1, 1);
  size_t vote_queue_size_in_node2 = node2->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size_in_node2, 0);
}

TEST_F(PbftRpcTest, vote_broadcast) {
  // set nodes account balance
  val_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  vector<FullNodeConfig> cfgs;
  for (auto i = 1; i <= 3; ++i) {
    cfgs.emplace_back(fmt("./core_tests/conf/conf_taraxa%s.json", i));
  }
  for (auto& cfg : cfgs) {
    for (auto& cfg_other : cfgs) {
      cfg.eth_chain_params.genesisState[addr(cfg_other.node_secret)] =
          dev::eth::Account(0, new_balance);
    }
    cfg.eth_chain_params.calculateStateRoot(true);
  }
  auto node_count = 0;
  auto node1(taraxa::FullNode::make(cfgs[node_count++]));
  auto node2(taraxa::FullNode::make(cfgs[node_count++]));
  auto node3(taraxa::FullNode::make(cfgs[node_count++]));
  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true);  // boot node
  node2->start(false);
  node3->start(false);

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
  ASSERT_GT(nw1->getPeerCount(), 0);
  ASSERT_GT(nw2->getPeerCount(), 0);
  ASSERT_GT(nw3->getPeerCount(), 0);

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();

  // generate vote
  blk_hash_t blockhash(1);
  blk_hash_t pbft_blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;
  Vote vote =
      node1->generateVote(blockhash, type, period, step, pbft_blockhash);

  node1->clearUnverifiedVotesTable();
  node2->clearUnverifiedVotesTable();
  node3->clearUnverifiedVotesTable();

  nw1->onNewPbftVote(vote);

  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

  size_t vote_queue_size1 = node1->getUnverifiedVotesSize();
  size_t vote_queue_size2 = node2->getUnverifiedVotesSize();
  size_t vote_queue_size3 = node3->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size1, 0);
  EXPECT_EQ(vote_queue_size2, 1);
  EXPECT_EQ(vote_queue_size3, 1);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityWarning;
  logOptions.includeChannels.push_back("NETWORK");
  logOptions.includeChannels.push_back("TARCAP");
  logOptions.includeChannels.push_back("VOTE_MGR");
  dev::setupLogging(logOptions);
  dev::db::setDatabaseKind(dev::db::DatabaseKind::RocksDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
