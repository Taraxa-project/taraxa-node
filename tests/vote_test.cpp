#include <gtest/gtest.h>
#include <libdevcore/SHA3.h>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "node/node.hpp"
#include "pbft/pbft_manager.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {
using namespace vrf_wrapper;

auto g_vrf_sk = Lazy([] {
  return vrf_sk_t(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
});
auto g_sk = Lazy([] {
  return secret_t("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                  dev::Secret::ConstructFromStringType::FromHex);
});
struct VoteTest : NodesTest {};

std::pair<PbftPeriod, PbftRound> clearAllVotes(const std::vector<std::shared_ptr<FullNode>> &nodes) {
  // Get highest round from all nodes
  PbftPeriod max_period = 0;
  PbftRound max_round = 1;
  for (const auto &node : nodes) {
    auto [node_round, node_period] = node->getPbftManager()->getPbftRoundAndPeriod();
    if (node_period > max_period) {
      max_period = node_period;
    }
    if (node_period == max_period && node_round > max_round) {
      max_round = node_round;
    }
  }

  // Clean up votes from db & memory for each node
  for (const auto &node : nodes) {
    // Clear unverified votes and verified votes table
    auto db = node->getDB();
    auto vote_mgr = node->getVoteManager();
    auto verified_votes = db->getVerifiedVotes();

    auto batch = db->createWriteBatch();
    for (auto const &v : verified_votes) {
      db->removeVerifiedVoteToBatch(v->getHash(), batch);
    }
    db->commitWriteBatch(batch);

    vote_mgr->cleanupVotesByPeriod(max_period);

    if (vote_mgr->getVerifiedVotesSize()) {
      vote_mgr->cleanupVotesByRound(max_period, max_round + 1);
    }
  }

  return {max_period, max_round};
}

TEST_F(VoteTest, verified_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  auto [period, round] = clearAllVotes({node});
  std::cout << "[TODO REMOVE] Clear all votes returned period " << period << ", round " << round << std::endl;

  // Generate a vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = PbftVoteTypes::soft_vote;
  PbftStep step = 2;
  auto vote = pbft_mgr->generateVote(blockhash, type, period, round, step);
  vote->calculateWeight(1, 1, 1);

  auto vote_mgr = node->getVoteManager();
  vote_mgr->addVerifiedVote(vote);
  EXPECT_TRUE(vote_mgr->voteInVerifiedMap(vote));
  // Test same vote cannot add twice
  vote_mgr->addVerifiedVote(vote);
  EXPECT_EQ(vote_mgr->getVerifiedVotesSize(), 1);
  EXPECT_EQ(vote_mgr->getVerifiedVotes().size(), 1);

  auto [period2, round2] = clearAllVotes({node});
  std::cout << "[TODO REMOVE] Clear all votes returned period " << period2 << ", round " << round2 << std::endl;

  EXPECT_FALSE(vote_mgr->voteInVerifiedMap(vote));
  EXPECT_EQ(vote_mgr->getVerifiedVotesSize(), 0);
  EXPECT_EQ(vote_mgr->getVerifiedVotes().size(), 0);
}

// CONCERN TODO This test does not test period and round based cleanup differences at all..

// Add votes round 1, 2 and 3 into unverified vote table
// Verify votes by round 2, will remove round 1 in the table, and keep round 2 & 3 votes
TEST_F(VoteTest, DISABLED_add_cleanup_get_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  clearAllVotes({node});

  // generate 6 votes, 3 periods, 2 votes in round 1 each
  auto vote_mgr = node->getVoteManager();
  blk_hash_t voted_block_hash(1);
  PbftVoteTypes type = PbftVoteTypes::next_vote;
  for (int i = 1; i <= 3; i++) {
    for (int j = 1; j <= 2; j++) {
      PbftPeriod period = i;
      PbftRound round = 1;
      PbftStep step = 3 + j;
      auto vote = pbft_mgr->generateVote(voted_block_hash, type, period, round, step);
      vote->calculateWeight(1, 1, 1);
      vote_mgr->addVerifiedVote(vote);
    }
  }

  // Test add vote
  size_t votes_size = vote_mgr->getVerifiedVotesSize();
  EXPECT_EQ(votes_size, 6);

  // Test cleanup votes
  vote_mgr->cleanupVotesByRound(1, 2);  // cleanup more
  auto verified_votes_size = vote_mgr->getVerifiedVotesSize();
  EXPECT_EQ(verified_votes_size, 3);
  auto votes = vote_mgr->getVerifiedVotes();
  EXPECT_EQ(votes.size(), 3);
  for (auto const &v : votes) {
    EXPECT_GT(v->getRound(), 1);
  }

  vote_mgr->cleanupVotesByRound(2, 2);  // cleanup more
  verified_votes_size = vote_mgr->getVerifiedVotesSize();
  EXPECT_EQ(verified_votes_size, 0);
  votes = vote_mgr->getVerifiedVotes();
  EXPECT_TRUE(votes.empty());
}

TEST_F(VoteTest, round_determine_from_next_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  clearAllVotes({node});

  auto vote_mgr = node->getVoteManager();
  size_t two_t_plus_one = 2;

  // Generate votes in 3 rounds, 2 steps, each step have 3 votes
  blk_hash_t voted_block_hash(1);
  PbftVoteTypes type = PbftVoteTypes::next_vote;
  for (int i = 10; i <= 12; i++) {
    for (int j = 4; j <= 5; j++) {
      PbftPeriod period = i;
      PbftRound round = i;
      PbftStep step = j;
      auto vote = pbft_mgr->generateVote(voted_block_hash, type, period, round, step);
      vote->calculateWeight(3, 3, 3);
      vote_mgr->addVerifiedVote(vote);
    }
  }

  auto new_round = vote_mgr->determineRoundFromPeriodAndVotes(12, two_t_plus_one);
  EXPECT_EQ(new_round->first, 13);
}

TEST_F(VoteTest, reconstruct_votes) {
  public_t pk(12345);
  sig_t sortition_sig(1234567);
  sig_t vote_sig(9878766);
  blk_hash_t propose_blk_hash(111111);
  PbftVoteTypes type(PbftVoteTypes::propose_vote);
  PbftPeriod period(999);
  PbftRound round(999);
  PbftStep step(1);
  VrfPbftMsg msg(type, period, round, step);
  VrfPbftSortition vrf_sortition(g_vrf_sk, msg);
  Vote vote1(g_sk, vrf_sortition, propose_blk_hash);
  auto rlp = vote1.rlp();
  Vote vote2(rlp);
  EXPECT_EQ(vote1, vote2);
}

// Generate a vote, send the vote from node2 to node1
TEST_F(VoteTest, transfer_vote) {
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

  clearAllVotes({node1, node2});

  // generate a vote far ahead (never exist in PBFT manager)
  blk_hash_t propose_block_hash(11);
  PbftVoteTypes type = PbftVoteTypes::propose_vote;
  PbftPeriod period = 1;
  PbftRound round = 1;
  PbftStep step = 1;
  auto vote = pbft_mgr1->generateVote(propose_block_hash, type, period, round, step);

  nw1->getSpecificHandler<network::tarcap::VotePacketHandler>()->sendPbftVote(nw1->getPeer(nw2->getNodeId()), vote,
                                                                              nullptr);

  auto vote_mgr1 = node1->getVoteManager();
  auto vote_mgr2 = node2->getVoteManager();
  EXPECT_HAPPENS({60s, 100ms}, [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, vote_mgr2->getVerifiedVotesSize(), 1) });
  EXPECT_EQ(vote_mgr1->getVerifiedVotesSize(), 0);
}

TEST_F(VoteTest, vote_broadcast) {
  auto node_cfgs = make_node_cfgs(3);
  auto nodes = launch_nodes(node_cfgs);
  auto &node1 = nodes[0];
  auto &node2 = nodes[1];
  auto &node3 = nodes[2];

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();
  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

  auto [period, round] = clearAllVotes({node1, node2, node3});

  // generate a vote far ahead (never exist in PBFT manager)
  blk_hash_t propose_block_hash(111);
  PbftVoteTypes type = PbftVoteTypes::propose_vote;
  PbftStep step = 1;
  auto vote = pbft_mgr1->generateVote(propose_block_hash, type, period, round, step);

  node1->getNetwork()->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVote(vote, nullptr);

  auto vote_mgr1 = node1->getVoteManager();
  auto vote_mgr2 = node2->getVoteManager();
  auto vote_mgr3 = node3->getVoteManager();
  EXPECT_HAPPENS({60s, 100ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, vote_mgr2->getVerifiedVotesSize(), 1)
    WAIT_EXPECT_EQ(ctx, vote_mgr3->getVerifiedVotesSize(), 1)
  });
  EXPECT_EQ(vote_mgr1->getVerifiedVotesSize(), 0);
}

TEST_F(VoteTest, previous_round_next_votes) {
  auto node_cfgs = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfgs);
  auto &node = nodes[0];

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  // Clear unverfied/verified table/DB
  clearAllVotes({node});

  // Clear next votes structure
  auto next_votes_mgr = node->getNextVotesManager();
  next_votes_mgr->clearVotes();

  const auto chain_size = node->getPbftChain()->getPbftChainSize();
  auto pbft_2t_plus_1 = pbft_mgr->getPbftTwoTPlusOne(chain_size).value();
  EXPECT_EQ(pbft_2t_plus_1, 1);

  // Generate a vote voted at kNullBlockHash
  PbftVoteTypes type = PbftVoteTypes::next_vote;
  PbftPeriod period = 1;
  PbftRound round = 1;
  PbftStep step = 4;
  blk_hash_t voted_pbft_block_hash(0);
  auto vote1 = pbft_mgr->generateVote(voted_pbft_block_hash, type, period, round, step);
  vote1->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes_1{vote1};

  // Enough votes for kNullBlockHash
  next_votes_mgr->addNextVotes(next_votes_1, pbft_2t_plus_1);
  EXPECT_TRUE(next_votes_mgr->haveEnoughVotesForNullBlockHash());
  EXPECT_EQ(next_votes_mgr->getNextVotes().size(), next_votes_1.size());
  EXPECT_EQ(next_votes_mgr->getNextVotesWeight(), next_votes_1.size());

  // Generate a vote voted at value blk_hash_t(1)
  voted_pbft_block_hash = blk_hash_t(1);
  step = 5;
  auto vote2 = pbft_mgr->generateVote(voted_pbft_block_hash, type, period, round, step);
  vote2->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes_2{vote2};

  // Enough votes for blk_hash_t(1)
  next_votes_mgr->updateWithSyncedVotes(next_votes_2, pbft_2t_plus_1);
  EXPECT_EQ(next_votes_mgr->getVotedValue(), voted_pbft_block_hash);
  EXPECT_TRUE(next_votes_mgr->enoughNextVotes());
  auto expect_size = next_votes_1.size() + next_votes_2.size();
  EXPECT_EQ(expect_size, 2);
  EXPECT_EQ(next_votes_mgr->getNextVotes().size(), expect_size);
  EXPECT_EQ(next_votes_mgr->getNextVotesWeight(), expect_size);

  // Copy next_votes_1 and next_votes_2 into next_votes_3
  std::vector<std::shared_ptr<Vote>> next_votes_3;
  next_votes_3.reserve(expect_size);
  next_votes_3.insert(next_votes_3.end(), next_votes_1.begin(), next_votes_1.end());
  next_votes_3.insert(next_votes_3.end(), next_votes_2.begin(), next_votes_2.end());
  EXPECT_EQ(next_votes_3.size(), 2);

  // Should not update anything
  next_votes_mgr->addNextVotes(next_votes_3, pbft_2t_plus_1);
  EXPECT_EQ(next_votes_mgr->getNextVotes().size(), expect_size);
  EXPECT_EQ(next_votes_mgr->getNextVotesWeight(), expect_size);

  // Should not update anything
  next_votes_mgr->updateWithSyncedVotes(next_votes_3, pbft_2t_plus_1);
  EXPECT_EQ(next_votes_mgr->getNextVotes().size(), next_votes_3.size());
  EXPECT_EQ(next_votes_mgr->getNextVotesWeight(), next_votes_3.size());

  // Generate a vote voted at value blk_hash_t(2)
  voted_pbft_block_hash = blk_hash_t(2);
  period = 2;
  round = 2;
  auto vote3 = pbft_mgr->generateVote(voted_pbft_block_hash, type, period, round, step);
  vote3->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes_4{vote3};

  next_votes_mgr->updateNextVotes(next_votes_4, pbft_2t_plus_1);
  EXPECT_FALSE(next_votes_mgr->haveEnoughVotesForNullBlockHash());
  EXPECT_EQ(next_votes_mgr->getVotedValue(), voted_pbft_block_hash);
  EXPECT_FALSE(next_votes_mgr->enoughNextVotes());
  EXPECT_EQ(next_votes_mgr->getNextVotes().size(), next_votes_4.size());
  EXPECT_EQ(next_votes_mgr->getNextVotesWeight(), next_votes_4.size());
}

TEST_F(VoteTest, vote_count_compare) {
  auto vote_count_old = [](u256 balance, u256 threshold) { return u256(balance / threshold); };
  auto vote_count_new = [](u256 balance, u256 threshold, u256 step) {
    // the same logic as new GO method. Result should be the same as for the old method if threshold == step
    u256 res = 0;
    if (balance >= threshold) {
      res = balance - threshold;
      res /= step;
      res += 1;
    }
    return res;
  };

  {
    auto balance = 1000000;
    auto threshold = 100000;
    EXPECT_EQ(vote_count_old(balance, threshold), vote_count_new(balance, threshold, threshold));
  }

  {
    auto balance = 1000000000000;
    auto threshold = 100000;
    EXPECT_EQ(vote_count_old(balance, threshold), vote_count_new(balance, threshold, threshold));
  }

  {
    auto balance = u256("10000000000000000000000000");
    auto threshold = 100000;
    EXPECT_EQ(vote_count_old(balance, threshold), vote_count_new(balance, threshold, threshold));
  }

  {
    auto step = 100000;
    auto threshold = 1000000;
    auto balance = u256(7 * step);
    EXPECT_EQ(vote_count_old(balance, threshold), vote_count_new(balance, threshold, threshold));
  }
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
