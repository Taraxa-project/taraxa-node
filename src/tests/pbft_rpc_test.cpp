#include <gtest/gtest.h>
#include <libdevcore/SHA3.h>

#include "../full_node.hpp"
#include "../network.hpp"
#include "../pbft_manager.hpp"
#include "../static_init.hpp"
#include "../util_test/util.hpp"

namespace taraxa::core_tests {
using namespace vrf_wrapper;

auto g_vrf_sk = Lazy([] {
  return vrf_sk_t(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
});
auto g_sk =
    Lazy([] { return secret_t("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex); });
struct PbftRpcTest : BaseTest {};

TEST_F(PbftRpcTest, full_node_lambda_input_test) {
  auto node_cfgs = make_node_cfgs(1);
  FullNode::Handle node(node_cfgs[0]);

  node->start();
  auto pbft_mgr = node->getPbftManager();
  EXPECT_EQ(pbft_mgr->LAMBDA_ms_MIN, 2000);
}

// Add votes round 1, 2 and 3 into unverified vote table
// Get votes round 2, will remove round 1 in the table, and return round 2 & 3
// votes
TEST_F(PbftRpcTest, add_cleanup_get_votes) {
  auto node_cfgs = make_node_cfgs(1);
  FullNode::Handle node(node_cfgs[0]);

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  std::shared_ptr<VoteManager> vote_mgr = node->getVoteManager();
  node->getVoteManager()->clearUnverifiedVotesTable();

  // generate 6 votes, each round has 2 votes
  blk_hash_t blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  blk_hash_t pbft_chain_last_block_hash = node->getPbftChain()->getLastPbftBlockHash();
  for (int i = 1; i <= 3; i++) {
    for (int j = 1; j <= 2; j++) {
      uint64_t round = i;
      size_t step = j;
      Vote vote = node->getPbftManager()->generateVote(blockhash, type, round, step, pbft_chain_last_block_hash);
      node->getVoteManager()->addVote(vote);
    }
  }
  // Test add vote
  size_t votes_size = node->getVoteManager()->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 6);

  // Test get votes
  // CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
  size_t valid_sortition_players = 1;
  pbft_mgr->setSortitionThreshold(valid_sortition_players);
  uint64_t pbft_round = 2;
  std::vector<Vote> votes = vote_mgr->getVotes(pbft_round, pbft_chain_last_block_hash, pbft_mgr->getSortitionThreshold(), valid_sortition_players,
                                               [](...) { return true; });
  EXPECT_EQ(votes.size(), 4);
  for (Vote const &v : votes) {
    EXPECT_GT(v.getRound(), 1);
  }

  // Test cleanup votes
  votes_size = node->getVoteManager()->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 4);
  vote_mgr->cleanupVotes(4);  // cleanup round 2 & 3
  votes_size = node->getVoteManager()->getUnverifiedVotesSize();
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
  VrfPbftMsg msg(blk_hash_t(123), type, round, step);
  VrfPbftSortition vrf_sortition(g_vrf_sk, msg);
  Vote vote1(g_sk, vrf_sortition, blk_hash);
  auto rlp = vote1.rlp();
  Vote vote2(rlp);
  EXPECT_EQ(vote1, vote2);
}

// Generate a vote, send the vote from node2 to node1
TEST_F(PbftRpcTest, transfer_vote) {
  auto node_cfgs = make_node_cfgs(2);
  auto nodes = launch_nodes(node_cfgs);
  auto &node1 = nodes[0];
  auto &node2 = nodes[1];
  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  pbft_mgr1->stop();
  pbft_mgr2->stop();

  // generate vote
  blk_hash_t blockhash(11);
  blk_hash_t pbft_blockhash(991);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;

  Vote vote = node2->getPbftManager()->generateVote(blockhash, type, period, step, pbft_blockhash);

  node1->getVoteManager()->clearUnverifiedVotesTable();
  node2->getVoteManager()->clearUnverifiedVotesTable();

  nw2->sendPbftVote(nw1->getNodeId(), vote);
  taraxa::thisThreadSleepForMilliSeconds(100);

  size_t vote_queue_size_in_node1 = node1->getVoteManager()->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size_in_node1, 1);
  size_t vote_queue_size_in_node2 = node2->getVoteManager()->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size_in_node2, 0);
}

TEST_F(PbftRpcTest, vote_broadcast) {
  auto node_cfgs = make_node_cfgs(3);
  auto nodes = launch_nodes(node_cfgs);
  auto &node1 = nodes[0];
  auto &node2 = nodes[1];
  auto &node3 = nodes[2];

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();

  int node_peers = 2;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers && nw2->getPeerCount() == node_peers && nw3->getPeerCount() == node_peers) {
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
  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

  // generate vote
  blk_hash_t blockhash(1);
  blk_hash_t pbft_blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;
  Vote vote = node1->getPbftManager()->generateVote(blockhash, type, period, step, pbft_blockhash);

  node1->getVoteManager()->clearUnverifiedVotesTable();
  node2->getVoteManager()->clearUnverifiedVotesTable();
  node3->getVoteManager()->clearUnverifiedVotesTable();

  nw1->onNewPbftVote(vote);
  taraxa::thisThreadSleepForMilliSeconds(100);

  size_t vote_queue_size1 = node1->getVoteManager()->getUnverifiedVotesSize();
  size_t vote_queue_size2 = node2->getVoteManager()->getUnverifiedVotesSize();
  size_t vote_queue_size3 = node3->getVoteManager()->getUnverifiedVotesSize();
  EXPECT_EQ(vote_queue_size1, 0);
  EXPECT_EQ(vote_queue_size2, 1);
  EXPECT_EQ(vote_queue_size3, 1);
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  taraxa::static_init();
  LoggingConfig logging;
  logging.verbosity = taraxa::VerbosityError;
  logging.channels["NETWORK"] = taraxa::VerbosityError;
  logging.channels["TARCAP"] = taraxa::VerbosityError;
  logging.channels["VOTE_MGR"] = taraxa::VerbosityError;
  addr_t node_addr;
  setupLoggingConfiguration(node_addr, logging);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
