#include <gtest/gtest.h>
#include <libdevcore/SHA3.h>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "node/node.hpp"
#include "pbft/pbft_manager.hpp"
#include "util_test/util.hpp"

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
struct VoteTest : BaseTest {};

void clearAllVotes(std::shared_ptr<FullNode> &node) {
  // Clear unverified votes and verified votes table
  auto db = node->getDB();
  auto vote_mgr = node->getVoteManager();
  auto verified_votes = db->getVerifiedVotes();

  auto batch = db->createWriteBatch();
  for (auto const &v : verified_votes) {
    db->removeVerifiedVoteToBatch(v->getHash(), batch);
  }
  db->commitWriteBatch(batch);

  vote_mgr->clearUnverifiedVotesTable();
  vote_mgr->clearVerifiedVotesTable();
}

TEST_F(VoteTest, unverified_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  clearAllVotes(node);

  // Generate a vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  auto round = 1;
  auto step = 1;
  auto vote = pbft_mgr->generateVote(blockhash, type, round, step);

  auto vote_mgr = node->getVoteManager();
  vote_mgr->addUnverifiedVote(vote);
  EXPECT_TRUE(vote_mgr->voteInUnverifiedMap(vote->getRound(), vote->getHash()));

  // Generate 3 votes, (round = 1, step = 1) is duplicate
  std::vector<std::shared_ptr<Vote>> unverified_votes;
  for (auto i = 1; i <= 3; i++) {
    round = i;
    step = i;
    auto vote = pbft_mgr->generateVote(blockhash, type, round, step);
    unverified_votes.emplace_back(vote);
  }

  vote_mgr->moveVerifyToUnverify(unverified_votes);
  EXPECT_EQ(vote_mgr->getUnverifiedVotes().size(), unverified_votes.size());
  EXPECT_EQ(vote_mgr->getUnverifiedVotesSize(), unverified_votes.size());

  vote_mgr->removeUnverifiedVote(unverified_votes[0]->getRound(), unverified_votes[0]->getHash());
  EXPECT_FALSE(vote_mgr->voteInUnverifiedMap(unverified_votes[0]->getRound(), unverified_votes[0]->getHash()));
  EXPECT_EQ(vote_mgr->getUnverifiedVotes().size(), unverified_votes.size() - 1);
  EXPECT_EQ(vote_mgr->getUnverifiedVotesSize(), unverified_votes.size() - 1);

  vote_mgr->clearUnverifiedVotesTable();
  EXPECT_TRUE(vote_mgr->getUnverifiedVotes().empty());
  EXPECT_EQ(vote_mgr->getUnverifiedVotesSize(), 0);
}

TEST_F(VoteTest, verified_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  clearAllVotes(node);

  // Generate a vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = soft_vote_type;
  auto round = 1;
  auto step = 2;
  auto vote = pbft_mgr->generateVote(blockhash, type, round, step);
  vote->calculateWeight(1, 1, 1);

  auto vote_mgr = node->getVoteManager();
  vote_mgr->addVerifiedVote(vote);
  EXPECT_TRUE(vote_mgr->voteInVerifiedMap(vote));
  // Test same vote cannot add twice
  vote_mgr->addVerifiedVote(vote);
  EXPECT_EQ(vote_mgr->getVerifiedVotesSize(), 1);
  EXPECT_EQ(vote_mgr->getVerifiedVotes().size(), 1);

  vote_mgr->clearVerifiedVotesTable();
  EXPECT_FALSE(vote_mgr->voteInVerifiedMap(vote));
  EXPECT_EQ(vote_mgr->getVerifiedVotesSize(), 0);
  EXPECT_EQ(vote_mgr->getVerifiedVotes().size(), 0);
}

// Test moving all verified votes to unverified table/DB
TEST_F(VoteTest, remove_verified_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  clearAllVotes(node);

  auto db = node->getDB();
  auto vote_mgr = node->getVoteManager();

  // Generate 3 votes and add into verified table
  std::vector<std::shared_ptr<Vote>> votes;
  blk_hash_t blockhash(1);
  PbftVoteTypes type = next_vote_type;
  for (auto i = 1; i <= 3; i++) {
    auto round = i;
    auto step = i;
    auto vote = pbft_mgr->generateVote(blockhash, type, round, step);
    vote->calculateWeight(1, 1, 1);
    votes.emplace_back(vote);
    db->saveVerifiedVote(vote);
    vote_mgr->addVerifiedVote(vote);
    EXPECT_TRUE(vote_mgr->voteInVerifiedMap(vote));
  }
  EXPECT_EQ(vote_mgr->getVerifiedVotesSize(), votes.size());

  vote_mgr->removeVerifiedVotes();

  EXPECT_EQ(vote_mgr->getVerifiedVotesSize(), 0);
  EXPECT_EQ(vote_mgr->getVerifiedVotes().size(), 0);
  EXPECT_TRUE(db->getVerifiedVotes().empty());
  EXPECT_EQ(vote_mgr->getUnverifiedVotesSize(), votes.size());
}

// Add votes round 1, 2 and 3 into unverified vote table
// Verify votes by round 2, will remove round 1 in the table, and keep round 2 & 3 votes
TEST_F(VoteTest, add_cleanup_get_votes) {
  auto node = create_nodes(1, true /*start*/).front();

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  clearAllVotes(node);

  // generate 6 votes, each round has 2 votes
  auto vote_mgr = node->getVoteManager();
  blk_hash_t voted_block_hash(1);
  PbftVoteTypes type = next_vote_type;
  for (int i = 1; i <= 3; i++) {
    for (int j = 1; j <= 2; j++) {
      uint64_t round = i;
      size_t step = 3 + j;
      auto vote = pbft_mgr->generateVote(voted_block_hash, type, round, step);
      vote_mgr->addUnverifiedVote(vote);
    }
  }

  // Test add vote
  size_t votes_size = vote_mgr->getUnverifiedVotesSize();
  EXPECT_EQ(votes_size, 6);

  // Test Verify votes
  // CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
  size_t valid_sortition_players = 1;
  pbft_mgr->setSortitionThreshold(valid_sortition_players);
  uint64_t pbft_round = 2;
  vote_mgr->verifyVotes(pbft_round, [&pbft_mgr, valid_sortition_players](auto const &v) {
    try {
      v->validate(1, valid_sortition_players, pbft_mgr->getSortitionThreshold());
    } catch (const std::logic_error &e) {
      return false;
    }
    return true;
  });
  auto verified_votes_size = vote_mgr->getVerifiedVotesSize();
  EXPECT_EQ(verified_votes_size, 4);
  auto votes = vote_mgr->getVerifiedVotes();
  EXPECT_EQ(votes.size(), 4);
  for (auto const &v : votes) {
    EXPECT_GT(v->getRound(), 1);
  }

  // Test cleanup votes
  vote_mgr->cleanupVotes(4);  // cleanup round 2 & 3
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

  clearAllVotes(node);

  auto vote_mgr = node->getVoteManager();
  size_t two_t_plus_one = 2;

  // Generate votes in 3 rounds, 2 steps, each step have 3 votes
  blk_hash_t voted_block_hash(1);
  PbftVoteTypes type = next_vote_type;
  for (int i = 10; i <= 12; i++) {
    for (int j = 4; j <= 5; j++) {
      uint64_t round = i;
      size_t step = j;
      auto vote = pbft_mgr->generateVote(voted_block_hash, type, round, step);
      vote->calculateWeight(3, 3, 3);
      vote_mgr->addVerifiedVote(vote);
    }
  }

  auto new_round = vote_mgr->roundDeterminedFromVotes(two_t_plus_one);
  EXPECT_EQ(new_round, 13);
}

TEST_F(VoteTest, reconstruct_votes) {
  public_t pk(12345);
  sig_t sortition_sig(1234567);
  sig_t vote_sig(9878766);
  blk_hash_t propose_blk_hash(111111);
  PbftVoteTypes type(propose_vote_type);
  uint64_t round(999);
  size_t step(2);
  VrfPbftMsg msg(type, round, step);
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

  clearAllVotes(node1);
  clearAllVotes(node2);

  // generate a vote far ahead (never exist in PBFT manager)
  blk_hash_t propose_block_hash(11);
  PbftVoteTypes type = next_vote_type;
  uint64_t period = 999;
  size_t step = 1000;
  auto vote = pbft_mgr2->generateVote(propose_block_hash, type, period, step);

  std::static_pointer_cast<network::tarcap::VotePacketHandler>(
      nw2->getSpecificHandler(network::tarcap::SubprotocolPacketType::VotePacket))
      ->sendPbftVotes(nw1->getNodeId(), {vote});

  auto vote_mgr1 = node1->getVoteManager();
  auto vote_mgr2 = node2->getVoteManager();
  EXPECT_HAPPENS({60s, 100ms}, [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, vote_mgr1->getUnverifiedVotesSize(), 1) });
  EXPECT_EQ(vote_mgr2->getUnverifiedVotesSize(), 0);
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

  clearAllVotes(node1);
  clearAllVotes(node2);
  clearAllVotes(node3);

  // generate a vote far ahead (never exist in PBFT manager)
  blk_hash_t propose_block_hash(111);
  PbftVoteTypes type = next_vote_type;
  uint64_t period = 1000;
  size_t step = 1002;
  auto vote = pbft_mgr1->generateVote(propose_block_hash, type, period, step);

  node1->getNetwork()->onNewPbftVotes(std::vector{vote});

  auto vote_mgr1 = node1->getVoteManager();
  auto vote_mgr2 = node2->getVoteManager();
  auto vote_mgr3 = node3->getVoteManager();
  EXPECT_HAPPENS({60s, 100ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, vote_mgr2->getUnverifiedVotesSize(), 1)
    WAIT_EXPECT_EQ(ctx, vote_mgr3->getUnverifiedVotesSize(), 1)
  });
  EXPECT_EQ(vote_mgr1->getUnverifiedVotesSize(), 0);
}

TEST_F(VoteTest, previous_round_next_votes) {
  auto node_cfgs = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfgs);
  auto &node = nodes[0];

  // stop PBFT manager, that will place vote
  auto pbft_mgr = node->getPbftManager();
  pbft_mgr->stop();

  // Clear unverfied/verified table/DB
  clearAllVotes(node);

  // Clear next votes structure
  auto next_votes_mgr = node->getNextVotesManager();
  next_votes_mgr->clear();

  auto pbft_2t_plus_1 = pbft_mgr->getTwoTPlusOne();
  EXPECT_EQ(pbft_2t_plus_1, 1);

  // Generate a vote voted at NULL_BLOCK_HASH
  PbftVoteTypes type = next_vote_type;
  auto round = 1;
  auto step = 4;
  blk_hash_t voted_pbft_block_hash(0);
  auto vote1 = pbft_mgr->generateVote(voted_pbft_block_hash, type, round, step);
  vote1->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes_1{vote1};

  // Enough votes for NULL_BLOCK_HASH
  next_votes_mgr->addNextVotes(next_votes_1, pbft_2t_plus_1);
  EXPECT_TRUE(next_votes_mgr->haveEnoughVotesForNullBlockHash());
  EXPECT_EQ(next_votes_mgr->getNextVotes().size(), next_votes_1.size());
  EXPECT_EQ(next_votes_mgr->getNextVotesWeight(), next_votes_1.size());

  // Generate a vote voted at value blk_hash_t(1)
  voted_pbft_block_hash = blk_hash_t(1);
  step = 5;
  auto vote2 = pbft_mgr->generateVote(voted_pbft_block_hash, type, round, step);
  vote2->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes_2{vote2};

  // Enough votes for blk_hash_t(1)
  auto db = node->getDB();
  auto batch = db->createWriteBatch();
  db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold, 1, batch);
  db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, 0, batch);
  db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount, 1, batch);
  db->commitWriteBatch(batch);
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
  round = 2;
  auto vote3 = pbft_mgr->generateVote(voted_pbft_block_hash, type, round, step);
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
