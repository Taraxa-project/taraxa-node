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

  // Clean up votes from memory for each node
  for (const auto &node : nodes) {
    node->getVoteManager()->cleanupVotesByPeriod(max_period + 1);
  }

  return {max_period, max_round};
}

TEST_F(VoteTest, verified_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  node->getPbftManager()->stop();

  auto [period, round] = clearAllVotes({node});
  std::cout << "[TODO REMOVE] Clear all votes returned period " << period << ", round " << round << std::endl;

  // Generate a vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = PbftVoteTypes::soft_vote;
  PbftStep step = 2;
  auto vote = node->getVoteManager()->generateVote(blockhash, type, period, round, step);
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

TEST_F(VoteTest, round_determine_from_next_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  node->getPbftManager()->stop();

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
      auto vote = vote_mgr->generateVote(voted_block_hash, type, period, round, step);
      vote->calculateWeight(3, 3, 3);
      vote_mgr->addVerifiedVote(vote);
    }
  }

  auto new_round = vote_mgr->determineNewRound(12, two_t_plus_one);
  EXPECT_EQ(*new_round, 13);
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
  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();

  clearAllVotes({node1, node2});

  // generate a vote far ahead (never exist in PBFT manager)
  blk_hash_t propose_block_hash(11);
  PbftVoteTypes type = PbftVoteTypes::propose_vote;
  PbftPeriod period = 1;
  PbftRound round = 1;
  PbftStep step = 1;
  auto vote = node1->getVoteManager()->generateVote(propose_block_hash, type, period, round, step);

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

  auto vote_mgr1 = node1->getVoteManager();
  auto vote_mgr2 = node2->getVoteManager();
  auto vote_mgr3 = node3->getVoteManager();

  auto [period, round] = clearAllVotes({node1, node2, node3});

  EXPECT_EQ(vote_mgr1->getVerifiedVotesSize(), 0);
  EXPECT_EQ(vote_mgr2->getVerifiedVotesSize(), 0);
  EXPECT_EQ(vote_mgr3->getVerifiedVotesSize(), 0);

  // generate a vote far ahead (never exist in PBFT manager)
  auto vote = vote_mgr1->generateVote(blk_hash_t(1), PbftVoteTypes::soft_vote, period, round, 2);

  node1->getNetwork()->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVote(vote, nullptr);

  EXPECT_HAPPENS({60s, 100ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, vote_mgr2->getVerifiedVotesSize(), 1)
    WAIT_EXPECT_EQ(ctx, vote_mgr3->getVerifiedVotesSize(), 1)
  });
  EXPECT_EQ(vote_mgr1->getVerifiedVotesSize(), 0);
}

TEST_F(VoteTest, two_t_plus_one_votes) {
  auto node_cfgs = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfgs);
  auto &node = nodes[0];

  // stop PBFT manager, that will place vote
  node->getPbftManager()->stop();

  auto vote_mgr = node->getVoteManager();

  // Clear unverfied/verified table/DB
  clearAllVotes({node});

  const auto chain_size = node->getPbftChain()->getPbftChainSize();
  auto pbft_2t_plus_1 = vote_mgr->getPbftTwoTPlusOne(chain_size).value();
  EXPECT_EQ(pbft_2t_plus_1, 1);

  auto genVote = [vote_mgr](PbftVoteTypes type, PbftPeriod period, PbftRound round, PbftStep step,
                            blk_hash_t block_hash = blk_hash_t(1)) {
    auto vote = vote_mgr->generateVote(block_hash, type, period, round, step);
    vote->calculateWeight(1, 1, 1);
    return vote;
  };

  // Generate a vote voted at kNullBlockHash
  PbftPeriod period = 1;
  PbftRound round = 1;

  vote_mgr->addVerifiedVote(genVote(PbftVoteTypes::soft_vote, period, round, 2));
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock).has_value());
  EXPECT_FALSE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::CertVotedBlock).has_value());
  EXPECT_FALSE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedBlock).has_value());
  EXPECT_FALSE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedNullBlock).has_value());

  vote_mgr->addVerifiedVote(genVote(PbftVoteTypes::cert_vote, period, round, 3));
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock).has_value());
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::CertVotedBlock).has_value());
  EXPECT_FALSE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedBlock).has_value());
  EXPECT_FALSE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedNullBlock).has_value());

  vote_mgr->addVerifiedVote(genVote(PbftVoteTypes::next_vote, period, round, 4));
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock).has_value());
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::CertVotedBlock).has_value());
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedBlock).has_value());
  EXPECT_FALSE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedNullBlock).has_value());

  vote_mgr->addVerifiedVote(genVote(PbftVoteTypes::next_vote, period, round, 5, kNullBlockHash));
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock).has_value());
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::CertVotedBlock).has_value());
  EXPECT_TRUE(vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedBlock).has_value());
  EXPECT_TRUE(
      vote_mgr->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::NextVotedNullBlock).has_value());
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
