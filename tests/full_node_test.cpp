
#include "node/full_node.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "common/static_init.hpp"
#include "consensus/pbft_manager.hpp"
#include "dag/dag.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
#include "network/rpc/Taraxa.h"
#include "string"
#include "transaction_manager/transaction_manager.hpp"
#include "util_test/samples.hpp"

// TODO rename this namespace to `tests`
namespace taraxa::core_tests {
using samples::sendTrx;

const unsigned NUM_TRX = 200;
const unsigned SYNC_TIMEOUT = 400;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_trx_signed_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });
auto g_mock_dag0 = Lazy([] { return samples::createMockDag0(); });

void send_2_nodes_trxs() {
  std::string sendtrx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 500,
                                      "number": 600,
                                      "nonce": 0,
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777)";
  std::string sendtrx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 700,
                                      "number": 400,
                                      "nonce": 600 ,
                                      "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e"}]}' 0.0.0.0:7778)";
  std::cout << "Sending trxs ..." << std::endl;
  std::thread t1([sendtrx1]() { EXPECT_FALSE(system(sendtrx1.c_str())); });
  std::thread t2([sendtrx2]() { EXPECT_FALSE(system(sendtrx2.c_str())); });

  t1.join();
  t2.join();
  std::cout << "All trxs sent..." << std::endl;
}

void send_dummy_trx() {
  std::string dummy_trx =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": 0,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 2004,
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777 > /dev/null)";

  std::cout << "Send dummy transaction ..." << std::endl;
  EXPECT_FALSE(system(dummy_trx.c_str()));
}

struct FullNodeTest : BaseTest {};

TEST_F(FullNodeTest, db_test) {
  auto db_ptr = std::make_shared<DbStorage>(data_dir);
  auto &db = *db_ptr;
  DagBlock blk1(blk_hash_t(1), 1, {}, {trx_hash_t(1), trx_hash_t(2)}, sig_t(777), blk_hash_t(0xB1), addr_t(999));
  DagBlock blk2(blk_hash_t(1), 1, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(777), blk_hash_t(0xB2), addr_t(999));
  DagBlock blk3(blk_hash_t(0xB1), 2, {}, {trx_hash_t(5)}, sig_t(777), blk_hash_t(0xB6), addr_t(999));
  // DAG
  db.saveDagBlock(blk1);
  db.saveDagBlock(blk2);
  db.saveDagBlock(blk3);
  EXPECT_EQ(blk1, *db.getDagBlock(blk1.getHash()));
  EXPECT_EQ(blk2, *db.getDagBlock(blk2.getHash()));
  EXPECT_EQ(blk3, *db.getDagBlock(blk3.getHash()));
  EXPECT_EQ(db.getBlocksByLevel(1), blk1.getHash().toString() + "," + blk2.getHash().toString());
  EXPECT_EQ(db.getBlocksByLevel(2), blk3.getHash().toString());

  // Transaction
  db.saveTransaction(g_trx_signed_samples[0]);
  db.saveTransaction(g_trx_signed_samples[1]);
  auto batch = db.createWriteBatch();
  db.addTransactionToBatch(g_trx_signed_samples[2], batch);
  db.addTransactionToBatch(g_trx_signed_samples[3], batch);
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[0].getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[1].getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[2].getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[3].getHash()));
  EXPECT_EQ(g_trx_signed_samples[0], *db.getTransaction(g_trx_signed_samples[0].getHash()));
  EXPECT_EQ(g_trx_signed_samples[1], *db.getTransaction(g_trx_signed_samples[1].getHash()));
  EXPECT_EQ(g_trx_signed_samples[2], *db.getTransaction(g_trx_signed_samples[2].getHash()));
  EXPECT_EQ(g_trx_signed_samples[3], *db.getTransaction(g_trx_signed_samples[3].getHash()));

  // PBFT manager round and step
  EXPECT_EQ(db.getPbftMgrField(PbftMgrRoundStep::PbftRound), 1);
  EXPECT_EQ(db.getPbftMgrField(PbftMgrRoundStep::PbftStep), 1);
  uint64_t pbft_round = 30;
  size_t pbft_step = 31;
  db.savePbftMgrField(PbftMgrRoundStep::PbftRound, pbft_round);
  db.savePbftMgrField(PbftMgrRoundStep::PbftStep, pbft_step);
  EXPECT_EQ(db.getPbftMgrField(PbftMgrRoundStep::PbftRound), pbft_round);
  EXPECT_EQ(db.getPbftMgrField(PbftMgrRoundStep::PbftStep), pbft_step);
  pbft_round = 90;
  pbft_step = 91;
  batch = db.createWriteBatch();
  db.addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftRound, pbft_round, batch);
  db.addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftStep, pbft_step, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbftMgrField(PbftMgrRoundStep::PbftRound), pbft_round);
  EXPECT_EQ(db.getPbftMgrField(PbftMgrRoundStep::PbftStep), pbft_step);

  // PBFT 2t+1
  db.savePbft2TPlus1(10, 3);
  EXPECT_EQ(db.getPbft2TPlus1(10), 3);
  batch = db.createWriteBatch();
  db.addPbft2TPlus1ToBatch(10, 6, batch);
  db.addPbft2TPlus1ToBatch(11, 3, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbft2TPlus1(10), 6);
  EXPECT_EQ(db.getPbft2TPlus1(11), 3);

  // PBFT manager status
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::soft_voted_block_in_round));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::executed_block));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::executed_in_round));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::next_voted_soft_value));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::next_voted_null_block_hash));
  db.savePbftMgrStatus(PbftMgrStatus::soft_voted_block_in_round, true);
  db.savePbftMgrStatus(PbftMgrStatus::executed_block, true);
  db.savePbftMgrStatus(PbftMgrStatus::executed_in_round, true);
  db.savePbftMgrStatus(PbftMgrStatus::next_voted_soft_value, true);
  db.savePbftMgrStatus(PbftMgrStatus::next_voted_null_block_hash, true);
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::soft_voted_block_in_round));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::executed_block));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::executed_in_round));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::next_voted_soft_value));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::next_voted_null_block_hash));
  batch = db.createWriteBatch();
  db.addPbftMgrStatusToBatch(PbftMgrStatus::soft_voted_block_in_round, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::executed_block, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::executed_in_round, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_soft_value, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::next_voted_null_block_hash, false, batch);
  db.commitWriteBatch(batch);
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::soft_voted_block_in_round));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::executed_block));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::executed_in_round));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::next_voted_soft_value));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::next_voted_null_block_hash));

  // PBFT manager voted value
  EXPECT_EQ(db.getPbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round), nullptr);
  EXPECT_EQ(db.getPbftMgrVotedValue(PbftMgrVotedValue::soft_voted_block_hash_in_round), nullptr);
  db.savePbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round, blk_hash_t(1));
  db.savePbftMgrVotedValue(PbftMgrVotedValue::soft_voted_block_hash_in_round, blk_hash_t(2));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round), blk_hash_t(1));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::soft_voted_block_hash_in_round), blk_hash_t(2));
  batch = db.createWriteBatch();
  db.addPbftMgrVotedValueToBatch(PbftMgrVotedValue::own_starting_value_in_round, blk_hash_t(4), batch);
  db.addPbftMgrVotedValueToBatch(PbftMgrVotedValue::soft_voted_block_hash_in_round, blk_hash_t(5), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::own_starting_value_in_round), blk_hash_t(4));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::soft_voted_block_hash_in_round), blk_hash_t(5));

  // PBFT cert voted block hash
  EXPECT_EQ(db.getPbftCertVotedBlockHash(1), nullptr);
  db.savePbftCertVotedBlockHash(1, blk_hash_t(1));
  EXPECT_EQ(*db.getPbftCertVotedBlockHash(1), blk_hash_t(1));
  batch = db.createWriteBatch();
  db.addPbftCertVotedBlockHashToBatch(1, blk_hash_t(2), batch);
  db.addPbftCertVotedBlockHashToBatch(2, blk_hash_t(3), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(*db.getPbftCertVotedBlockHash(1), blk_hash_t(2));
  EXPECT_EQ(*db.getPbftCertVotedBlockHash(2), blk_hash_t(3));

  // PBFT cert voted block
  auto pbft_block1 = make_simple_pbft_block(blk_hash_t(1), 1);
  auto pbft_block2 = make_simple_pbft_block(blk_hash_t(2), 2);
  auto pbft_block3 = make_simple_pbft_block(blk_hash_t(3), 3);
  auto pbft_block4 = make_simple_pbft_block(blk_hash_t(4), 4);
  EXPECT_EQ(db.getPbftCertVotedBlock(blk_hash_t(1)), nullptr);
  db.savePbftCertVotedBlock(pbft_block1);
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block1.getBlockHash())->rlp(false), pbft_block1.rlp(false));
  batch = db.createWriteBatch();
  db.addPbftCertVotedBlockToBatch(pbft_block2, batch);
  db.addPbftCertVotedBlockToBatch(pbft_block3, batch);
  db.addPbftCertVotedBlockToBatch(pbft_block4, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block2.getBlockHash())->rlp(false), pbft_block2.rlp(false));
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block3.getBlockHash())->rlp(false), pbft_block3.rlp(false));
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block4.getBlockHash())->rlp(false), pbft_block4.rlp(false));

  // pbft_blocks
  EXPECT_FALSE(db.pbftBlockInDb(blk_hash_t(0)));
  EXPECT_FALSE(db.pbftBlockInDb(blk_hash_t(1)));
  pbft_block1 = make_simple_pbft_block(blk_hash_t(1), 2);
  pbft_block2 = make_simple_pbft_block(blk_hash_t(2), 3);
  pbft_block3 = make_simple_pbft_block(blk_hash_t(3), 4);
  pbft_block4 = make_simple_pbft_block(blk_hash_t(4), 5);
  batch = db.createWriteBatch();
  db.addPbftBlockToBatch(pbft_block1, batch);
  db.addPbftBlockToBatch(pbft_block2, batch);
  db.addPbftBlockToBatch(pbft_block3, batch);
  db.addPbftBlockToBatch(pbft_block4, batch);
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block1.getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block2.getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block3.getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block4.getBlockHash()));
  EXPECT_EQ(db.getPbftBlock(pbft_block1.getBlockHash())->rlp(false), pbft_block1.rlp(false));
  EXPECT_EQ(db.getPbftBlock(pbft_block2.getBlockHash())->rlp(false), pbft_block2.rlp(false));
  EXPECT_EQ(db.getPbftBlock(pbft_block3.getBlockHash())->rlp(false), pbft_block3.rlp(false));
  EXPECT_EQ(db.getPbftBlock(pbft_block4.getBlockHash())->rlp(false), pbft_block4.rlp(false));

  // pbft_blocks (head)
  PbftChain pbft_chain(blk_hash_t(0), addr_t(), db_ptr);
  db.savePbftHead(pbft_chain.getHeadHash(), pbft_chain.getJsonStr());
  EXPECT_EQ(db.getPbftHead(pbft_chain.getHeadHash()), pbft_chain.getJsonStr());
  batch = db.createWriteBatch();
  pbft_chain.updatePbftChain(blk_hash_t(123));
  db.addPbftHeadToBatch(pbft_chain.getHeadHash(), pbft_chain.getJsonStr(), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbftHead(pbft_chain.getHeadHash()), pbft_chain.getJsonStr());
  batch = db.createWriteBatch();
  db.addPbftHeadToBatch(pbft_chain.getHeadHash(), pbft_chain.getJsonStr(), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbftHead(pbft_chain.getHeadHash()), pbft_chain.getJsonStr());

  // status
  db.saveStatusField(StatusDbField::TrxCount, 5);
  db.saveStatusField(StatusDbField::ExecutedBlkCount, 6);
  EXPECT_EQ(db.getStatusField(StatusDbField::TrxCount), 5);
  EXPECT_EQ(db.getStatusField(StatusDbField::ExecutedBlkCount), 6);
  batch = db.createWriteBatch();
  db.addStatusFieldToBatch(StatusDbField::ExecutedBlkCount, 10, batch);
  db.addStatusFieldToBatch(StatusDbField::ExecutedTrxCount, 20, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getStatusField(StatusDbField::ExecutedBlkCount), 10);
  EXPECT_EQ(db.getStatusField(StatusDbField::ExecutedTrxCount), 20);

  // Unverified votes
  std::vector<Vote> unverified_votes = db.getUnverifiedVotes();
  EXPECT_TRUE(unverified_votes.empty());
  EXPECT_FALSE(db.unverifiedVoteExist(blk_hash_t(0)));
  blk_hash_t voted_pbft_block_hash(1);
  auto weighted_index = 0;
  for (auto i = 0; i < 3; i++) {
    auto round = i;
    auto step = i;
    VrfPbftMsg msg(soft_vote_type, round, step, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    unverified_votes.emplace_back(vote);
  }
  db.saveUnverifiedVote(unverified_votes[0]);
  EXPECT_TRUE(db.unverifiedVoteExist(unverified_votes[0].getHash()));
  EXPECT_EQ(db.getUnverifiedVote(unverified_votes[0].getHash())->rlp(true), unverified_votes[0].rlp(true));
  batch = db.createWriteBatch();
  db.addUnverifiedVoteToBatch(unverified_votes[1], batch);
  db.addUnverifiedVoteToBatch(unverified_votes[2], batch);
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.unverifiedVoteExist(unverified_votes[1].getHash()));
  EXPECT_TRUE(db.unverifiedVoteExist(unverified_votes[2].getHash()));
  EXPECT_EQ(db.getUnverifiedVotes().size(), unverified_votes.size());
  batch = db.createWriteBatch();
  db.removeUnverifiedVoteToBatch(unverified_votes[0].getHash(), batch);
  db.removeUnverifiedVoteToBatch(unverified_votes[1].getHash(), batch);
  db.commitWriteBatch(batch);
  EXPECT_FALSE(db.unverifiedVoteExist(unverified_votes[0].getHash()));
  EXPECT_FALSE(db.unverifiedVoteExist(unverified_votes[1].getHash()));
  EXPECT_EQ(db.getUnverifiedVotes().size(), unverified_votes.size() - 2);

  // Verified votes
  std::vector<Vote> verified_votes = db.getVerifiedVotes();
  EXPECT_TRUE(verified_votes.empty());
  voted_pbft_block_hash = blk_hash_t(2);
  weighted_index = 0;
  for (auto i = 0; i < 3; i++) {
    auto round = i;
    auto step = i;
    VrfPbftMsg msg(next_vote_type, round, step, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    verified_votes.emplace_back(vote);
  }
  db.saveVerifiedVote(verified_votes[0]);
  EXPECT_EQ(db.getVerifiedVote(verified_votes[0].getHash())->rlp(true), verified_votes[0].rlp(true));
  batch = db.createWriteBatch();
  db.addVerifiedVoteToBatch(verified_votes[1], batch);
  db.addVerifiedVoteToBatch(verified_votes[2], batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getVerifiedVotes().size(), verified_votes.size());
  batch = db.createWriteBatch();
  for (size_t i = 0; i < verified_votes.size(); i++) {
    db.removeVerifiedVoteToBatch(verified_votes[i].getHash(), batch);
  }
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.getVerifiedVotes().empty());

  // Soft votes
  auto round = 1, step = 2;
  std::vector<Vote> soft_votes = db.getSoftVotes(round);
  EXPECT_TRUE(soft_votes.empty());
  weighted_index = 0;
  for (auto i = 0; i < 3; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(soft_vote_type, round, step, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    soft_votes.emplace_back(vote);
  }
  db.saveSoftVotes(round, soft_votes);
  auto soft_votes_from_db = db.getSoftVotes(round);
  EXPECT_EQ(soft_votes.size(), soft_votes_from_db.size());
  EXPECT_EQ(soft_votes_from_db.size(), 3);
  for (auto i = 3; i < 5; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(soft_vote_type, round, step, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    soft_votes.emplace_back(vote);
  }
  batch = db.createWriteBatch();
  db.addSoftVotesToBatch(round, soft_votes, batch);
  db.commitWriteBatch(batch);
  soft_votes_from_db = db.getSoftVotes(round);
  EXPECT_EQ(soft_votes.size(), soft_votes_from_db.size());
  EXPECT_EQ(soft_votes_from_db.size(), 5);
  batch = db.createWriteBatch();
  db.removeSoftVotesToBatch(round, batch);
  db.commitWriteBatch(batch);
  soft_votes_from_db = db.getSoftVotes(round);
  EXPECT_TRUE(soft_votes_from_db.empty());

  // Certified votes
  std::vector<Vote> cert_votes;
  voted_pbft_block_hash = blk_hash_t(10);
  weighted_index = 0;
  for (auto i = 0; i < 3; i++) {
    weighted_index = i;
    VrfPbftMsg msg(cert_vote_type, 2, 3, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    cert_votes.emplace_back(vote);
  }
  batch = db.createWriteBatch();
  db.addCertVotesToBatch(voted_pbft_block_hash, cert_votes, batch);
  db.commitWriteBatch(batch);
  auto pbft_block = make_simple_pbft_block(voted_pbft_block_hash, 1);
  PbftBlockCert pbft_block_cert_votes(pbft_block, cert_votes);
  auto cert_votes_from_db = db.getCertVotes(voted_pbft_block_hash);
  PbftBlockCert pbft_block_cert_votes_from_db(pbft_block, cert_votes_from_db);
  EXPECT_EQ(pbft_block_cert_votes.rlp(), pbft_block_cert_votes_from_db.rlp());

  // Next votes
  round = 3, step = 5;
  weighted_index = 0;
  std::vector<Vote> next_votes = db.getNextVotes(round);
  EXPECT_TRUE(next_votes.empty());
  for (auto i = 0; i < 3; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(next_vote_type, round, step, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    next_votes.emplace_back(vote);
  }
  db.saveNextVotes(round, next_votes);
  auto next_votes_from_db = db.getNextVotes(round);
  EXPECT_EQ(next_votes.size(), next_votes_from_db.size());
  EXPECT_EQ(next_votes_from_db.size(), 3);
  next_votes.clear();
  for (auto i = 3; i < 5; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(next_vote_type, round, step, weighted_index);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    next_votes.emplace_back(vote);
  }
  batch = db.createWriteBatch();
  db.addNextVotesToBatch(round, next_votes, batch);
  db.commitWriteBatch(batch);
  next_votes_from_db = db.getNextVotes(round);
  EXPECT_EQ(next_votes.size(), next_votes_from_db.size());
  EXPECT_EQ(next_votes_from_db.size(), 2);
  batch = db.createWriteBatch();
  db.removeNextVotesToBatch(round, batch);
  db.commitWriteBatch(batch);
  next_votes_from_db = db.getNextVotes(round);
  EXPECT_TRUE(next_votes_from_db.empty());

  // period_pbft_block
  batch = db.createWriteBatch();
  db.addPbftBlockPeriodToBatch(1, blk_hash_t(1), batch);
  db.addPbftBlockPeriodToBatch(2, blk_hash_t(2), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(*db.getPeriodPbftBlock(1), blk_hash_t(1));
  EXPECT_EQ(*db.getPeriodPbftBlock(2), blk_hash_t(2));

  // dag_block_period
  batch = db.createWriteBatch();
  db.addDagBlockPeriodToBatch(blk_hash_t(1), 1, batch);
  db.addDagBlockPeriodToBatch(blk_hash_t(2), 2, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(1, *db.getDagBlockPeriod(blk_hash_t(1)));
  EXPECT_EQ(2, *db.getDagBlockPeriod(blk_hash_t(2)));

  // DPOS proposal period DAG levels status
  EXPECT_EQ(0, db.getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus::max_proposal_period));
  db.saveDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus::max_proposal_period, 5);
  EXPECT_EQ(5, db.getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus::max_proposal_period));
  batch = db.createWriteBatch();
  db.addDposProposalPeriodLevelsFieldToBatch(DposProposalPeriodLevelsStatus::max_proposal_period, 10, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(10, db.getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus::max_proposal_period));

  // DPOS proposal period DAG levels map
  EXPECT_TRUE(db.getProposalPeriodDagLevelsMap(0).empty());
  ProposalPeriodDagLevelsMap proposal_period_0_levels;
  db.saveProposalPeriodDagLevelsMap(proposal_period_0_levels);
  auto period_0_levels_bytes = db.getProposalPeriodDagLevelsMap(0);
  EXPECT_FALSE(period_0_levels_bytes.empty());
  ProposalPeriodDagLevelsMap period_0_levels_from_db(period_0_levels_bytes);
  EXPECT_EQ(period_0_levels_from_db.proposal_period, 0);
  EXPECT_EQ(period_0_levels_from_db.levels_interval.first, 0);
  EXPECT_EQ(period_0_levels_from_db.levels_interval.second, proposal_period_0_levels.max_levels_per_period);
  EXPECT_EQ(period_0_levels_from_db.levels_interval.second, 100);
  batch = db.createWriteBatch();
  ProposalPeriodDagLevelsMap proposal_period_1_levels(1, 101, 110);
  db.addProposalPeriodDagLevelsMapToBatch(proposal_period_1_levels, batch);
  db.commitWriteBatch(batch);
  auto period_1_levels_bytes = db.getProposalPeriodDagLevelsMap(1);
  EXPECT_FALSE(period_1_levels_bytes.empty());
  ProposalPeriodDagLevelsMap period_1_levels_from_db(period_1_levels_bytes);
  EXPECT_EQ(period_1_levels_from_db.proposal_period, 1);
  EXPECT_EQ(period_1_levels_from_db.levels_interval.first, 101);
  EXPECT_EQ(period_1_levels_from_db.levels_interval.second, 110);
}

TEST_F(FullNodeTest, sync_five_nodes) {
  using namespace std;

  auto node_cfgs = make_node_cfgs<20>(5);
  auto nodes = launch_nodes(node_cfgs);

  class context {
    decltype(nodes) &nodes_;
    vector<TransactionClient> trx_clients;
    uint64_t issued_trx_count = 0;
    unordered_map<addr_t, val_t> expected_balances;
    shared_mutex m;

   public:
    context(decltype(nodes_) nodes) : nodes_(nodes) {
      expected_balances[nodes[0]->getAddress()] = own_effective_genesis_bal(nodes[0]->getConfig());
      for (auto const &node : nodes_) {
        trx_clients.emplace_back(node);
      }
    }

    auto getIssuedTrxCount() {
      shared_lock l(m);
      return issued_trx_count;
    }

    void dummy_transaction() {
      {
        unique_lock l(m);
        ++issued_trx_count;
      }
      auto result = trx_clients[0].coinTransfer(KeyPair::create().address(), 0, KeyPair::create(), false);
    }

    void coin_transfer(int sender_node_i, addr_t const &to, val_t const &amount, bool verify_executed = true) {
      {
        unique_lock l(m);
        ++issued_trx_count;
        expected_balances[to] += amount;
        expected_balances[nodes_[sender_node_i]->getAddress()] -= amount;
      }
      auto result = trx_clients[sender_node_i].coinTransfer(to, amount, {}, verify_executed);
      if (verify_executed)
        EXPECT_EQ(result.stage, TransactionClient::TransactionStage::executed);
      else
        EXPECT_EQ(result.stage, TransactionClient::TransactionStage::inserted);
    }

    void assert_balances_synced() {
      shared_lock l(m);
      for (auto &[addr, val] : expected_balances) {
        for (auto &node : nodes_) {
          ASSERT_EQ(val, node->getFinalChain()->getBalance(addr).first);
        }
      }
    }

  } context(nodes);

  // transfer some coins to your friends ...
  auto init_bal = own_effective_genesis_bal(nodes[0]->getConfig()) / nodes.size();

  {
    vector<thread> threads;
    uint16_t thread_completed = 0;
    std::mutex m;
    std::condition_variable cv;
    for (size_t i(1); i < nodes.size(); ++i) {
      threads.emplace_back([i, &context, &nodes, &thread_completed, &init_bal, &m, &cv] {
        context.coin_transfer(0, nodes[i]->getAddress(), init_bal, true);
        {
          std::unique_lock<std::mutex> lk(m);
          thread_completed++;
          if (thread_completed == nodes.size() - 1) {
            cv.notify_one();
          }
        }
      });
    }

    std::unique_lock<std::mutex> lk(m);
    auto node_size = nodes.size();
    while (!cv.wait_for(lk, 100ms, [&thread_completed, &context, &node_size] {
      if (thread_completed < node_size - 1) {
        context.dummy_transaction();
        return false;
      }
      return true;
    }))
      ;
    for (auto &t : threads) t.join();
  }

  std::cout << "Initial coin transfers from node 0 issued ... " << std::endl;

  {
    vector<thread> threads;
    vector<bool> thread_completed;
    for (size_t i(0); i < nodes.size(); ++i) thread_completed.push_back(false);
    for (size_t i(0); i < nodes.size(); ++i) {
      auto to = i < nodes.size() - 1 ? nodes[i + 1]->getAddress() : addr_t("d79b2575d932235d87ea2a08387ae489c31aa2c9");
      threads.emplace_back([i, to, &context, &thread_completed] {
        for (auto _(0); _ < 10; ++_) {
          context.coin_transfer(i, to, 100);
        }
        thread_completed[i] = true;
      });
    }
    wait({60s, 500ms}, [&](auto &ctx) {
      for (auto t : thread_completed) {
        if (!t) {
          context.dummy_transaction();
          ctx.fail();
          return;
        }
      }
    });
    for (auto &t : threads) t.join();
  }
  std::cout << "Issued transatnion count " << context.getIssuedTrxCount() << std::endl;

  auto TIMEOUT = SYNC_TIMEOUT;
  for (unsigned i = 0; i < TIMEOUT; i++) {
    auto num_vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
    auto num_vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
    auto num_vertices3 = nodes[2]->getDagManager()->getNumVerticesInDag();
    auto num_vertices4 = nodes[3]->getDagManager()->getNumVerticesInDag();
    auto num_vertices5 = nodes[4]->getDagManager()->getNumVerticesInDag();

    auto num_trx1 = nodes[0]->getTransactionManager()->getTransactionCount();
    auto num_trx2 = nodes[1]->getTransactionManager()->getTransactionCount();
    auto num_trx3 = nodes[2]->getTransactionManager()->getTransactionCount();
    auto num_trx4 = nodes[3]->getTransactionManager()->getTransactionCount();
    auto num_trx5 = nodes[4]->getTransactionManager()->getTransactionCount();

    auto issued_trx_count = context.getIssuedTrxCount();

    if (num_vertices1 == num_vertices2 && num_vertices2 == num_vertices3 && num_vertices3 == num_vertices4 &&
        num_vertices4 == num_vertices5 && num_trx1 == issued_trx_count && num_trx2 == issued_trx_count &&
        num_trx3 == issued_trx_count && num_trx4 == issued_trx_count && num_trx5 == issued_trx_count) {
      break;
    }

    if (i % 10 == 0) {
      std::cout << "Still waiting for DAG vertices and transaction gossiping to sync ..." << std::endl;
      std::cout << " Node 1: Dag size = " << num_vertices1.first << " Trx count = " << num_trx1 << std::endl
                << " Node 2: Dag size = " << num_vertices2.first << " Trx count = " << num_trx2 << std::endl
                << " Node 3: Dag size = " << num_vertices3.first << " Trx count = " << num_trx3 << std::endl
                << " Node 4: Dag size = " << num_vertices4.first << " Trx count = " << num_trx4 << std::endl
                << " Node 5: Dag size = " << num_vertices5.first << " Trx count = " << num_trx5
                << " Issued transaction count = " << issued_trx_count << std::endl;
      std::cout << "Send a dummy transaction to coverge DAG" << std::endl;
      context.dummy_transaction();
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  EXPECT_GT(nodes[0]->getNumProposedBlocks(), 2);
  EXPECT_GT(nodes[1]->getNumProposedBlocks(), 2);
  EXPECT_GT(nodes[2]->getNumProposedBlocks(), 2);
  EXPECT_GT(nodes[3]->getNumProposedBlocks(), 2);
  EXPECT_GT(nodes[4]->getNumProposedBlocks(), 2);

  ASSERT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), context.getIssuedTrxCount());
  ASSERT_EQ(nodes[1]->getTransactionManager()->getTransactionCount(), context.getIssuedTrxCount());
  ASSERT_EQ(nodes[2]->getTransactionManager()->getTransactionCount(), context.getIssuedTrxCount());
  ASSERT_EQ(nodes[3]->getTransactionManager()->getTransactionCount(), context.getIssuedTrxCount());
  ASSERT_EQ(nodes[4]->getTransactionManager()->getTransactionCount(), context.getIssuedTrxCount());

  auto issued_trx_count = context.getIssuedTrxCount();

  auto num_vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
  auto num_vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
  auto num_vertices3 = nodes[2]->getDagManager()->getNumVerticesInDag();
  auto num_vertices4 = nodes[3]->getDagManager()->getNumVerticesInDag();
  auto num_vertices5 = nodes[4]->getDagManager()->getNumVerticesInDag();

  EXPECT_EQ(num_vertices1, num_vertices2);
  EXPECT_EQ(num_vertices2, num_vertices3);
  EXPECT_EQ(num_vertices3, num_vertices4);
  EXPECT_EQ(num_vertices4, num_vertices5);

  ASSERT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), issued_trx_count);
  ASSERT_EQ(nodes[1]->getTransactionManager()->getTransactionCount(), issued_trx_count);
  ASSERT_EQ(nodes[2]->getTransactionManager()->getTransactionCount(), issued_trx_count);
  ASSERT_EQ(nodes[3]->getTransactionManager()->getTransactionCount(), issued_trx_count);
  ASSERT_EQ(nodes[4]->getTransactionManager()->getTransactionCount(), issued_trx_count);

  std::cout << "All transactions received ..." << std::endl;

  TIMEOUT = SYNC_TIMEOUT * 10;
  for (unsigned i = 0; i < TIMEOUT; i++) {
    auto trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();
    auto trx_executed2 = nodes[1]->getDB()->getNumTransactionExecuted();
    auto trx_executed3 = nodes[2]->getDB()->getNumTransactionExecuted();
    auto trx_executed4 = nodes[3]->getDB()->getNumTransactionExecuted();
    auto trx_executed5 = nodes[4]->getDB()->getNumTransactionExecuted();
    // unique trxs in DAG block
    auto trx_packed1 = nodes[0]->getDB()->getNumTransactionInDag();
    auto trx_packed2 = nodes[1]->getDB()->getNumTransactionInDag();
    auto trx_packed3 = nodes[2]->getDB()->getNumTransactionInDag();
    auto trx_packed4 = nodes[3]->getDB()->getNumTransactionInDag();
    auto trx_packed5 = nodes[4]->getDB()->getNumTransactionInDag();

    if (trx_packed1 < trx_executed1) {
      std::cout << "Warning! " << trx_packed1 << " packed transactions is less than " << trx_executed1
                << " executed transactions in node 1" << std::endl;
    }

    if (trx_executed1 == issued_trx_count && trx_executed2 == issued_trx_count && trx_executed3 == issued_trx_count &&
        trx_executed4 == issued_trx_count && trx_executed5 == issued_trx_count) {
      if (trx_packed1 == issued_trx_count && trx_packed2 == issued_trx_count && trx_packed3 == issued_trx_count &&
          trx_packed4 == issued_trx_count && trx_packed5 == issued_trx_count) {
        break;
      } else {
        std::cout << "Warning! See all nodes with " << issued_trx_count
                  << " executed transactions, "
                     "but not all nodes yet see"
                  << issued_trx_count << " packed transactions!!!";
      }
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
    if (i % 100 == 0) {
      if (trx_executed1 != issued_trx_count) {
        std::cout << " Node 1: executed blk= " << nodes[0]->getDB()->getNumBlockExecuted()
                  << " Dag size: " << nodes[0]->getDagManager()->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed1 << "/" << issued_trx_count << std::endl;
      }
      if (trx_executed2 != issued_trx_count) {
        std::cout << " Node 2: executed blk= " << nodes[1]->getDB()->getNumBlockExecuted()
                  << " Dag size: " << nodes[1]->getDagManager()->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed2 << "/" << issued_trx_count << std::endl;
      }
      if (trx_executed3 != issued_trx_count) {
        std::cout << " Node 3: executed blk= " << nodes[2]->getDB()->getNumBlockExecuted()
                  << " Dag size: " << nodes[2]->getDagManager()->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed3 << "/" << issued_trx_count << std::endl;
      }
      if (trx_executed4 != issued_trx_count) {
        std::cout << " Node 4: executed blk= " << nodes[3]->getDB()->getNumBlockExecuted()
                  << " Dag size: " << nodes[3]->getDagManager()->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed4 << "/" << issued_trx_count << std::endl;
      }
      if (trx_executed5 != issued_trx_count) {
        std::cout << " Node 5: executed blk= " << nodes[4]->getDB()->getNumBlockExecuted()
                  << " Dag size: " << nodes[4]->getDagManager()->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed5 << "/" << issued_trx_count << std::endl;
      }
    }
    if (i == TIMEOUT - 1) {
      // last wait
      std::cout << "Warning! Syncing maybe failed ..." << std::endl;
    }
  }

  auto k = 0;
  for (auto const &node : nodes) {
    k++;
    auto vertices_diff = node->getDagManager()->getNumVerticesInDag().first - 1 - node->getDB()->getNumBlockExecuted();
    if (vertices_diff >= nodes.size()                                      //
        || node->getDB()->getNumTransactionExecuted() != issued_trx_count  //
        || node->getDB()->getNumTransactionInDag() != issued_trx_count) {
      std::cout << "Node " << k << " :Number of trx packed = " << node->getDB()->getNumTransactionInDag() << std::endl;
      std::cout << "Node " << k << " :Number of trx executed = " << node->getDB()->getNumTransactionExecuted()
                << std::endl;

      std::cout << "Node " << k << " :Number of block executed = " << node->getDB()->getNumBlockExecuted() << std::endl;
      auto num_vertices = node->getDagManager()->getNumVerticesInDag();
      std::cout << "Node " << k << " :Number of vertices in Dag = " << num_vertices.first << " , "
                << num_vertices.second << std::endl;
      auto dags = getOrderedDagBlocks(node->getDB());
      for (size_t i(0); i < dags.size(); ++i) {
        auto d = dags[i];
        std::cout << i << " " << d << " trx: " << nodes[0]->getDagBlockManager()->getDagBlock(d)->getTrxs().size()
                  << std::endl;
      }
      std::string filename = "debug_dag_" + std::to_string(k);
      node->getDagManager()->drawGraph(filename);
    }
  }

  k = 0;
  for (auto const &node : nodes) {
    k++;

    EXPECT_EQ(node->getDB()->getNumTransactionExecuted(), issued_trx_count)
        << " \nNum executed in node " << k << " node " << node << " is : " << node->getDB()->getNumTransactionExecuted()
        << " \nNum linearized blks: " << getOrderedDagBlocks(node->getDB()).size()
        << " \nNum executed blks: " << node->getDB()->getNumBlockExecuted()
        << " \nNum vertices in DAG: " << node->getDagManager()->getNumVerticesInDag().first << " "
        << node->getDagManager()->getNumVerticesInDag().second << "\n";

    auto vertices_diff = node->getDagManager()->getNumVerticesInDag().first - 1 - node->getDB()->getNumBlockExecuted();

    // diff should be larger than 0 but smaller than number of nodes
    // genesis block won't be executed
    EXPECT_LT(vertices_diff, nodes.size())
        << "Failed in node " << k << "node " << node
        << " Number of vertices: " << node->getDagManager()->getNumVerticesInDag().first
        << " Number of executed blks: " << node->getDB()->getNumBlockExecuted() << std::endl;
    EXPECT_GE(vertices_diff, 0) << "Failed in node " << k << "node " << node
                                << " Number of vertices: " << node->getDagManager()->getNumVerticesInDag().first
                                << " Number of executed blks: " << node->getDB()->getNumBlockExecuted() << std::endl;
    EXPECT_EQ(node->getDB()->getNumTransactionInDag(), issued_trx_count);
  }

  context.assert_balances_synced();
}

TEST_F(FullNodeTest, insert_anchor_and_compute_order) {
  auto node_cfgs = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfgs);
  auto &node = nodes[0];

  g_mock_dag0 = samples::createMockDag1(node->getConfig().chain.dag_genesis_block.getHash().toString());

  for (int i = 1; i <= 9; i++) {
    node->getDagBlockManager()->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);
  blk_hash_t pivot;
  std::vector<blk_hash_t> tips;

  // -------- first period ----------

  node->getDagManager()->getLatestPivotAndTips(pivot, tips);
  uint64_t period;
  std::shared_ptr<vec_blk_t> order;
  std::tie(period, order) = node->getDagManager()->getDagBlockOrder(pivot);
  EXPECT_EQ(period, 1);
  EXPECT_EQ(order->size(), 6);

  if (order->size() == 6) {
    EXPECT_EQ((*order)[0], blk_hash_t(3));
    EXPECT_EQ((*order)[1], blk_hash_t(6));
    EXPECT_EQ((*order)[2], blk_hash_t(2));
    EXPECT_EQ((*order)[3], blk_hash_t(1));
    EXPECT_EQ((*order)[4], blk_hash_t(5));
    EXPECT_EQ((*order)[5], blk_hash_t(7));
  }
  auto write_batch = node->getDB()->createWriteBatch();
  auto num_blks_set = node->getDagManager()->setDagBlockOrder(pivot, period, *order, write_batch);
  node->getDB()->commitWriteBatch(write_batch);
  EXPECT_EQ(num_blks_set, 6);
  // -------- second period ----------

  for (int i = 10; i <= 16; i++) {
    node->getDagBlockManager()->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getDagManager()->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) = node->getDagManager()->getDagBlockOrder(pivot);
  EXPECT_EQ(period, 2);
  if (order->size() == 7) {
    EXPECT_EQ((*order)[0], blk_hash_t(11));
    EXPECT_EQ((*order)[1], blk_hash_t(10));
    EXPECT_EQ((*order)[2], blk_hash_t(13));
    EXPECT_EQ((*order)[3], blk_hash_t(9));
    EXPECT_EQ((*order)[4], blk_hash_t(12));
    EXPECT_EQ((*order)[5], blk_hash_t(14));
    EXPECT_EQ((*order)[6], blk_hash_t(15));
  }
  write_batch = node->getDB()->createWriteBatch();
  num_blks_set = node->getDagManager()->setDagBlockOrder(pivot, period, *order, write_batch);
  node->getDB()->commitWriteBatch(write_batch);
  EXPECT_EQ(num_blks_set, 7);

  // -------- third period ----------

  for (size_t i = 17; i < g_mock_dag0->size(); i++) {
    node->getDagBlockManager()->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getDagManager()->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) = node->getDagManager()->getDagBlockOrder(pivot);
  EXPECT_EQ(period, 3);
  if (order->size() == 5) {
    EXPECT_EQ((*order)[0], blk_hash_t(17));
    EXPECT_EQ((*order)[1], blk_hash_t(16));
    EXPECT_EQ((*order)[2], blk_hash_t(8));
    EXPECT_EQ((*order)[3], blk_hash_t(18));
    EXPECT_EQ((*order)[4], blk_hash_t(19));
  }
  write_batch = node->getDB()->createWriteBatch();
  num_blks_set = node->getDagManager()->setDagBlockOrder(pivot, period, *order, write_batch);
  node->getDB()->commitWriteBatch(write_batch);
  EXPECT_EQ(num_blks_set, 5);
}

TEST_F(FullNodeTest, destroy_db) {
  auto node_cfgs = make_node_cfgs(1);
  {
    FullNode::Handle node(node_cfgs[0]);
    auto db = node->getDB();
    db->saveTransaction(g_trx_signed_samples[0]);
    // Verify trx saved in db
    EXPECT_TRUE(db->getTransaction(g_trx_signed_samples[0].getHash()));
  }
  {
    FullNode::Handle node(node_cfgs[0]);
    auto db = node->getDB();
    // Verify trx saved in db after restart with destroy_db false
    EXPECT_TRUE(db->getTransaction(g_trx_signed_samples[0].getHash()));
  }
}

TEST_F(FullNodeTest, reconstruct_anchors) {
  auto node_cfgs = make_node_cfgs<5>(1);
  std::pair<blk_hash_t, blk_hash_t> anchors;
  {
    FullNode::Handle node(node_cfgs[0], true);

    taraxa::thisThreadSleepForMilliSeconds(500);

    TransactionClient trx_client(node);

    for (auto i = 0; i < 3; i++) {
      auto result = trx_client.coinTransfer(KeyPair::create().address(), 10, {}, false);
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
    anchors = node->getDagManager()->getAnchors();
  }
  {
    FullNode::Handle node(node_cfgs[0], true);

    taraxa::thisThreadSleepForMilliSeconds(500);

    EXPECT_EQ(anchors, node->getDagManager()->getAnchors());
  }
}  // namespace taraxa

TEST_F(FullNodeTest, reconstruct_dag) {
  auto node_cfgs = make_node_cfgs(1);

  unsigned long vertices1 = 0;
  unsigned long vertices2 = 0;
  unsigned long vertices3 = 0;
  unsigned long vertices4 = 0;

  auto num_blks = g_mock_dag0->size();
  {
    FullNode::Handle node(node_cfgs[0], true);
    g_mock_dag0 = samples::createMockDag0(node->getConfig().chain.dag_genesis_block.getHash().toString());

    taraxa::thisThreadSleepForMilliSeconds(100);

    for (size_t i = 1; i < num_blks; i++) {
      node->getDagBlockManager()->insertBlock(g_mock_dag0[i]);
    }

    taraxa::thisThreadSleepForMilliSeconds(100);
    vertices1 = node->getDagManager()->getNumVerticesInDag().first;
    EXPECT_EQ(vertices1, num_blks);
  }
  {
    FullNode::Handle node(node_cfgs[0], true);
    taraxa::thisThreadSleepForMilliSeconds(100);

    vertices2 = node->getDagManager()->getNumVerticesInDag().first;
    EXPECT_EQ(vertices2, num_blks);
  }
  {
    fs::remove_all(node_cfgs[0].db_path);
    FullNode::Handle node(node_cfgs[0], true);
    // TODO: pbft does not support node stop yet, to be fixed ...
    node->getPbftManager()->stop();
    for (size_t i = 1; i < num_blks; i++) {
      node->getDagBlockManager()->insertBlock(g_mock_dag0[i]);
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
    vertices3 = node->getDagManager()->getNumVerticesInDag().first;
  }
  {
    FullNode::Handle node(node_cfgs[0], true);
    taraxa::thisThreadSleepForMilliSeconds(100);
    vertices4 = node->getDagManager()->getNumVerticesInDag().first;
  }
  EXPECT_EQ(vertices1, vertices2);
  EXPECT_EQ(vertices2, vertices3);
  EXPECT_EQ(vertices3, vertices4);
}

TEST_F(FullNodeTest, sync_two_nodes1) {
  auto node_cfgs = make_node_cfgs<2, true>(2);
  auto nodes = launch_nodes(node_cfgs);

  // send 1000 trxs
  try {
    send_2_nodes_trxs();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  auto num_trx1 = nodes[0]->getTransactionManager()->getTransactionCount();
  auto num_trx2 = nodes[1]->getTransactionManager()->getTransactionCount();
  // add more delay if sync is not done
  for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_trx1 == 1000 && num_trx2 == 1000) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_trx1 = nodes[0]->getTransactionManager()->getTransactionCount();
    num_trx2 = nodes[1]->getTransactionManager()->getTransactionCount();
  }
  EXPECT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), 1000);
  EXPECT_EQ(nodes[1]->getTransactionManager()->getTransactionCount(), 1000);

  auto num_vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
  auto num_vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
  for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_vertices1.first > 3 && num_vertices2.first > 3 && num_vertices1 == num_vertices2) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
    num_vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
  }
  EXPECT_GE(num_vertices1.first, 3);
  EXPECT_EQ(num_vertices1, num_vertices2);
}

TEST_F(FullNodeTest, persist_counter) {
  auto node_cfgs = make_node_cfgs<2, true>(2);
  unsigned long num_exe_trx1 = 0, num_exe_trx2 = 0, num_exe_blk1 = 0, num_exe_blk2 = 0, num_trx1 = 0, num_trx2 = 0;
  {
    auto nodes = launch_nodes(node_cfgs);

    // send 1000 trxs
    try {
      send_2_nodes_trxs();
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    num_trx1 = nodes[0]->getTransactionManager()->getTransactionCount();
    num_trx2 = nodes[1]->getTransactionManager()->getTransactionCount();
    // add more delay if sync is not done
    for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
      if (num_trx1 == 1000 && num_trx2 == 1000) break;
      taraxa::thisThreadSleepForMilliSeconds(500);
      num_trx1 = nodes[0]->getTransactionManager()->getTransactionCount();
      num_trx2 = nodes[1]->getTransactionManager()->getTransactionCount();
    }
    EXPECT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), 1000);
    EXPECT_EQ(nodes[1]->getTransactionManager()->getTransactionCount(), 1000);
    std::cout << "All 1000 trxs are received ..." << std::endl;
    // time to make sure all transactions have been packed into block...
    // taraxa::thisThreadSleepForSeconds(10);
    taraxa::thisThreadSleepForMilliSeconds(2000);
    // send dummy trx to make sure all DAGs are ordered
    try {
      send_dummy_trx();
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    num_exe_trx1 = nodes[0]->getDB()->getNumTransactionExecuted();
    num_exe_trx2 = nodes[1]->getDB()->getNumTransactionExecuted();
    // add more delay if sync is not done
    for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
      if (num_exe_trx1 == 1001 && num_exe_trx2 == 1001) break;
      taraxa::thisThreadSleepForMilliSeconds(200);
      num_exe_trx1 = nodes[0]->getDB()->getNumTransactionExecuted();
      num_exe_trx2 = nodes[1]->getDB()->getNumTransactionExecuted();
    }

    ASSERT_EQ(num_exe_trx1, 1001);
    ASSERT_EQ(num_exe_trx2, 1001);

    num_exe_blk1 = nodes[0]->getDB()->getNumBlockExecuted();
    num_exe_blk2 = nodes[1]->getDB()->getNumBlockExecuted();

    num_trx1 = nodes[0]->getTransactionManager()->getTransactionCount();
    num_trx2 = nodes[1]->getTransactionManager()->getTransactionCount();

    EXPECT_GT(num_exe_blk1, 0);
    EXPECT_EQ(num_exe_blk1, num_exe_blk2);
    EXPECT_EQ(num_exe_trx1, num_trx1);
    EXPECT_EQ(num_exe_trx1, num_trx2);
  }
  {
    auto nodes = launch_nodes(node_cfgs);

    EXPECT_EQ(num_exe_trx1, nodes[0]->getDB()->getNumTransactionExecuted());
    EXPECT_EQ(num_exe_trx2, nodes[1]->getDB()->getNumTransactionExecuted());
    EXPECT_EQ(num_exe_blk1, nodes[0]->getDB()->getNumBlockExecuted());
    EXPECT_EQ(num_exe_blk2, nodes[1]->getDB()->getNumBlockExecuted());
    EXPECT_EQ(num_trx1, nodes[0]->getTransactionManager()->getTransactionCount());
    EXPECT_EQ(num_trx2, nodes[1]->getTransactionManager()->getTransactionCount());
  }
}

TEST_F(FullNodeTest, sync_two_nodes2) {
  auto node_cfgs = make_node_cfgs<20, true>(2);
  auto nodes = launch_nodes(node_cfgs);

  // send 1000 trxs
  try {
    std::cout << "Sending 1000 trxs ..." << std::endl;
    sendTrx(1000, 7777);
    std::cout << "1000 trxs sent ..." << std::endl;

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  auto vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
  auto vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
  // let nodes[1] sync nodes[0]
  for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
    if (vertices1 == vertices2 && vertices1.first > 3) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
    vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
  }
  EXPECT_GE(vertices1.first, 3);
  EXPECT_EQ(vertices1, vertices2);
}

TEST_F(FullNodeTest, single_node_run_two_transactions) {
  auto node_cfgs = make_node_cfgs<5, true>(1);
  FullNode::Handle node(node_cfgs[0], true);

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf868808502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba07659e8c7207a4b2cd96488108fed54c463b1719b438add1159beed04f6660da8a028feb0a3b44bd34e0dd608f82aeeb2cd70d1305653b5dc33678be2ffcfcac997"
                                      ]}' 0.0.0.0:7777)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf868018502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba05256492dd60623ab5a403ed1b508f845f87f631d2c2e7acd4357cd83ef5b6540a042def7cd4f3c25ce67ee25911740dab47161e096f1dd024badecec58888a890b"
                                      ]}' 0.0.0.0:7777)";

  std::cout << "Send first trx ..." << std::endl;
  EXPECT_FALSE(system(send_raw_trx1.c_str()));
  std::cout << "First trx received ..." << std::endl;

  EXPECT_HAPPENS({60s, 1s}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), 1)
    WAIT_EXPECT_EQ(ctx, node->getTransactionManager()->getTransactionCount(), 1)
    WAIT_EXPECT_EQ(ctx, node->getDagManager()->getNumVerticesInDag().first, 2)
  });

  std::cout << "First trx executed ..." << std::endl;
  std::cout << "Send second trx ..." << std::endl;
  EXPECT_FALSE(system(send_raw_trx2.c_str()));

  EXPECT_HAPPENS({60s, 1s}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), 2)
    WAIT_EXPECT_EQ(ctx, node->getTransactionManager()->getTransactionCount(), 2)
    WAIT_EXPECT_EQ(ctx, node->getDagManager()->getNumVerticesInDag().first, 3)
  });
}

TEST_F(FullNodeTest, two_nodes_run_two_transactions) {
  auto node_cfgs = make_node_cfgs<5, true>(2);
  auto nodes = launch_nodes(node_cfgs);

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf86b808502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca04cef8610e05b4476673c369204899da747a9b32b0ad3769814b620281c900408a0130499a83d0b56c184c6ac518f6bbe2f8f946b65bf42b08cfc9b4dfbe2ebfd04"
                                      ]}' 0.0.0.0:7777)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf86b018502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca06644c30a23286d0de8fa107f1bded3a7a214004042b2007b2a9a62c8b313cf79a06cbb522856838b107542d8213286928500b2822584d6c7c6fee3a2c348cade4a"
                                      ]}' 0.0.0.0:7777)";

  std::cout << "Send first trx ..." << std::endl;
  EXPECT_FALSE(system(send_raw_trx1.c_str()));
  std::cout << "First trx received ..." << std::endl;

  auto trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();

  for (unsigned i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();
    if (trx_executed1 == 1) break;
    thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), 1);
  EXPECT_GE(nodes[0]->getDagManager()->getNumVerticesInDag().first, 2);
  EXPECT_EQ(trx_executed1, 1);
  std::cout << "First trx executed ..." << std::endl;
  std::cout << "Send second trx ..." << std::endl;
  EXPECT_FALSE(system(send_raw_trx2.c_str()));

  trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();

  for (unsigned i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();
    if (trx_executed1 == 2) break;
    thisThreadSleepForMilliSeconds(1000);
  }
  EXPECT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), 2);
  EXPECT_GE(nodes[0]->getDagManager()->getNumVerticesInDag().first, 3);
  EXPECT_EQ(trx_executed1, 2);
}

TEST_F(FullNodeTest, save_network_to_file) {
  auto node_cfgs = make_node_cfgs(3);
  auto nodes = launch_nodes(node_cfgs);
  {
    FullNode::Handle node2(node_cfgs[1], true);
    FullNode::Handle node3(node_cfgs[2], true);

    for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (1 == node2->getNetwork()->getPeerCount() && 1 == node3->getNetwork()->getPeerCount()) break;
    }

    ASSERT_EQ(1, node2->getNetwork()->getPeerCount());
    ASSERT_EQ(1, node3->getNetwork()->getPeerCount());
  }
}

TEST_F(FullNodeTest, receive_send_transaction) {
  auto node_cfgs = make_node_cfgs<20, true>(1);
  FullNode::Handle node(node_cfgs[0], true);

  try {
    sendTrx(1000, 7777);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  std::cout << "1000 transaction are sent through RPC ..." << std::endl;

  auto num_proposed_blk = node->getNumProposedBlocks();
  for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_proposed_blk > 0) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_GT(node->getNumProposedBlocks(), 0);
}

TEST_F(FullNodeTest, detect_overlap_transactions) {
  auto node_cfgs = make_node_cfgs<20>(5);
  auto node_1_genesis_bal = own_effective_genesis_bal(node_cfgs[0]);
  auto nodes = launch_nodes(node_cfgs);
  // Even distribute coins from master boot node to other nodes. Since master
  // boot node owns whole coins, the active players should be only master boot
  // node at the moment.
  auto gas_price = val_t(2);
  auto data = bytes();
  auto nonce = 0;
  uint64_t trxs_count = 0;
  auto test_transfer_val = node_1_genesis_bal / node_cfgs.size();
  for (size_t i(1); i < nodes.size(); ++i) {
    Transaction master_boot_node_send_coins(nonce++, test_transfer_val, gas_price, 100000, data,
                                            nodes[0]->getSecretKey(), nodes[i]->getAddress());
    // broadcast trx and insert
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins, false, true);
    trxs_count++;
  }

  std::cout << "Checking all nodes executed transactions at initialization" << std::endl;
  wait({150s, 2s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions, expected " << trxs_count << std::endl;
        if (ctx.fail(); !ctx.is_last_attempt) {
          Transaction dummy_trx(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(), nodes[0]->getAddress());
          // broadcast dummy transaction
          nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false, true);
          trxs_count++;
          return;
        }
      }
    }
  });
  // Check account balance for each node
  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[0]->getAddress()).first,
              node_1_genesis_bal - (node_cfgs.size() - 1) * test_transfer_val);
    for (size_t j(1); j < nodes.size(); ++j) {
      // For node1 to node4 balances info on each node
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, test_transfer_val);
    }
  }

  // Sending coins in Robin Cycle
  auto send_coins = 1;
  for (size_t i(0); i < nodes.size(); ++i) {
    auto receiver_index = (i + 1) % nodes.size();
    // Each node sends 500 transactions
    auto j = 0;
    for (; j < 500; j++) {
      Transaction send_coins_in_robin_cycle(nonce++, send_coins, gas_price, 100000, data, nodes[i]->getSecretKey(),
                                            nodes[receiver_index]->getAddress());
      // broadcast trx and insert
      nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle, false, true);
      trxs_count++;
    }
    std::cout << "Node" << i << " sends " << j << " transactions to Node" << receiver_index << std::endl;
  }
  std::cout << "Checking all nodes execute transactions from robin cycle" << std::endl;

  wait({150s, 2s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions, expected " << trxs_count << std::endl;
        if (ctx.fail(); !ctx.is_last_attempt) {
          Transaction dummy_trx(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(), nodes[0]->getAddress());
          // broadcast dummy transaction
          nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false, true);
          trxs_count++;
          return;
        }
      }
    }
  });

  wait({30s, 1s}, [&](auto &ctx) {
    auto num_vertices0 = nodes[0]->getDagManager()->getNumVerticesInDag();
    auto num_vertices1 = nodes[1]->getDagManager()->getNumVerticesInDag();
    auto num_vertices2 = nodes[2]->getDagManager()->getNumVerticesInDag();
    auto num_vertices3 = nodes[3]->getDagManager()->getNumVerticesInDag();
    auto num_vertices4 = nodes[4]->getDagManager()->getNumVerticesInDag();
    if (num_vertices0 != num_vertices1 || num_vertices0 != num_vertices2 || num_vertices0 != num_vertices3 ||
        num_vertices0 != num_vertices4)
      ctx.fail();
  });

  // Check DAG
  auto num_vertices0 = nodes[0]->getDagManager()->getNumVerticesInDag();
  auto num_vertices1 = nodes[1]->getDagManager()->getNumVerticesInDag();
  auto num_vertices2 = nodes[2]->getDagManager()->getNumVerticesInDag();
  auto num_vertices3 = nodes[3]->getDagManager()->getNumVerticesInDag();
  auto num_vertices4 = nodes[4]->getDagManager()->getNumVerticesInDag();
  EXPECT_EQ(num_vertices0, num_vertices1);
  EXPECT_EQ(num_vertices1, num_vertices2);
  EXPECT_EQ(num_vertices2, num_vertices3);
  EXPECT_EQ(num_vertices3, num_vertices4);
  std::cout << "DAG size " << num_vertices0 << std::endl;

  // Check duplicate transactions in single one DAG block
  auto ordered_dag_blocks = getOrderedDagBlocks(nodes[0]->getDB());
  for (auto const &b : ordered_dag_blocks) {
    std::shared_ptr<DagBlock> block = nodes[0]->getDB()->getDagBlock(b);
    EXPECT_TRUE(block);
    vec_trx_t trxs_hash = block->getTrxs();
    std::unordered_set<trx_hash_t> unique_trxs;
    for (auto const &t : trxs_hash) {
      if (unique_trxs.count(t)) {
        EXPECT_FALSE(true) << "Duplicate transaction " << t << " in DAG block " << b << std::endl;
      }
      unique_trxs.insert(t);
    }
  }

  // Check executed duplicate transactions
  auto trxs_table = nodes[0]->getDB()->getAllTransactionStatus();
  EXPECT_EQ(trxs_count, trxs_table.size());
  std::unordered_set<trx_hash_t> unique_trxs;
  for (auto const &t : trxs_table) {
    if (unique_trxs.count(t.first)) {
      EXPECT_FALSE(true) << "Executed duplicate transacton " << t.first << std::endl;
    }
    unique_trxs.insert(t.first);
  }
}

TEST_F(FullNodeTest, db_rebuild) {
  uint64_t trxs_count = 0;
  auto trxs_count_at_pbft_size_5 = 0;
  auto executed_trxs = 0;
  auto executed_chain_size = 0;

  {
    auto node_cfgs = make_node_cfgs<5>(1);
    auto nodes = launch_nodes(node_cfgs);

    auto gas_price = val_t(2);
    auto data = bytes();
    auto nonce = 0;

    // Issue dummy trx until at least 10 pbft blocks created
    while (executed_chain_size < 10) {
      Transaction dummy_trx(nonce++, 0, gas_price, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                            nodes[0]->getAddress());
      nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false, true);
      trxs_count++;
      thisThreadSleepForMilliSeconds(100);
      executed_chain_size = nodes[0]->getFinalChain()->last_block_number();
      if (executed_chain_size == 5) {
        trxs_count_at_pbft_size_5 = nodes[0]->getDB()->getNumTransactionExecuted();
      }
    }
    executed_trxs = nodes[0]->getDB()->getNumTransactionExecuted();
    std::cout << "Executed transactions " << trxs_count_at_pbft_size_5 << " at chain size 5" << std::endl;
    std::cout << "Total executed transactions " << executed_trxs << std::endl;
    std::cout << "Executed chain size " << executed_chain_size << std::endl;

    std::cout << "Checking transactions executed" << std::endl;
    wait({60s, 1s}, [&](auto &ctx) {
      if (nodes[0]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node executed " << nodes[0]->getDB()->getNumTransactionExecuted() << " transactions, expected "
                  << trxs_count << std::endl;
        ctx.fail();
      }
    });
    executed_chain_size = nodes[0]->getFinalChain()->last_block_number();
    std::cout << "Executed transactions " << trxs_count_at_pbft_size_5 << " at chain size 5" << std::endl;
    std::cout << "Total executed transactions " << executed_trxs << std::endl;
    std::cout << "Executed chain size " << executed_chain_size << std::endl;
  }

  {
    std::cout << "Test rebuild DB" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    node_cfgs[0].test_params.rebuild_db = true;
    auto nodes = launch_nodes(node_cfgs);
  }

  {
    std::cout << "Check rebuild DB" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    auto nodes = launch_nodes(node_cfgs);
    EXPECT_EQ(nodes[0]->getDB()->getNumTransactionExecuted(), trxs_count);
    EXPECT_EQ(nodes[0]->getFinalChain()->last_block_number(), executed_chain_size);
  }

  {
    std::cout << "Test rebuild for period 5" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    node_cfgs[0].test_params.rebuild_db = true;
    node_cfgs[0].test_params.rebuild_db_period = 5;
    auto nodes = launch_nodes(node_cfgs);
  }

  {
    std::cout << "Check rebuild for period 5" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    auto nodes = launch_nodes(node_cfgs);
    EXPECT_EQ(nodes[0]->getDB()->getNumTransactionExecuted(), trxs_count_at_pbft_size_5);
    EXPECT_EQ(nodes[0]->getFinalChain()->last_block_number(), 5);
  }
}

TEST_F(FullNodeTest, transfer_to_self) {
  auto node_cfgs = make_node_cfgs<5, true>(3);
  auto nodes = launch_nodes(node_cfgs);

  std::cout << "Send first trx ..." << std::endl;
  auto node_addr = nodes[0]->getAddress();
  auto initial_bal = nodes[0]->getFinalChain()->getBalance(node_addr);
  uint64_t trx_count(100);
  EXPECT_TRUE(initial_bal.second);
  EXPECT_FALSE(system(fmt(R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0",
  "method": "create_test_coin_transactions",
  "params": [{
    "secret":"3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    "delay": 5,
    "number": %s,
    "nonce": 0,
    "receiver": "%s"}]}' 0.0.0.0:7777)",
                          trx_count, node_addr)
                          .data()));
  thisThreadSleepForSeconds(5);
  EXPECT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), trx_count);
  auto trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();
  send_dummy_trx();
  for (unsigned i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();
    if (trx_executed1 == trx_count + 1) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, trx_count + 1);
  auto const bal = nodes[0]->getFinalChain()->getBalance(node_addr);
  EXPECT_TRUE(bal.second);
  EXPECT_EQ(bal.first, initial_bal.first);
}

TEST_F(FullNodeTest, chain_config_json) {
  string expected_default_chain_cfg_json = R"({
  "dag_genesis_block": {
    "level": "0x0",
    "pivot": "0x0000000000000000000000000000000000000000000000000000000000000000",
    "sig": "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
    "timestamp": "0x5d422b80",
    "tips": [],
    "transactions": []
  },
  "final_chain": {
    "genesis_block_fields": {
      "author": "0x0000000000000000000000000000000000000000",
      "timestamp": "0x0"
    },
    "state": {
      "disable_block_rewards": true,
      "dpos": {
        "deposit_delay": "0x0",
        "eligibility_balance_threshold": "0x3b9aca00",
        "genesis_state": {
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
            "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x3b9aca00"
          }
        },
        "withdrawal_delay": "0x0"
      },
      "eth_chain_config": {
        "byzantium_block": "0x0",
        "constantinople_block": "0x0",
        "dao_fork_block": "0xffffffffffffffff",
        "eip_150_block": "0x0",
        "eip_158_block": "0x0",
        "homestead_block": "0x0",
        "petersburg_block": "0x0"
      },
      "execution_options": {
        "disable_gas_fee": true,
        "disable_nonce_check": true
      },
      "genesis_balances": {
        "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x1fffffffffffff"
      }
    }
  },
  "pbft": {
    "committee_size": "0x5",
    "dag_blocks_size": "0x64",
    "ghost_path_move_back": "0x1",
    "lambda_ms_min": "0x7d0",
    "run_count_votes": false
  },
  "vdf": {
    "difficulty_max" : "0x15",
		"difficulty_min" : "0x10",
    "difficulty_stale" : "0x16",
		"lambda_bound" : "0x64",
		"threshold_selection" : "0x8000",
		"threshold_vdf_omit" : "0x7200"
	}
})";
  Json::Value default_chain_config_json;
  istringstream(expected_default_chain_cfg_json) >> default_chain_config_json;
  ASSERT_EQ(default_chain_config_json, enc_json(ChainConfig::predefined()));
  Json::Value test_node_config_json;
  std::ifstream((DIR_CONF / "conf_taraxa1.json").string(), std::ifstream::binary) >> test_node_config_json;
  Json::Value test_node_wallet_json;
  std::ifstream((DIR_CONF / "wallet1.json").string(), std::ifstream::binary) >> test_node_wallet_json;
  test_node_config_json.removeMember("chain_config");
  ASSERT_EQ(enc_json(FullNodeConfig(test_node_config_json, test_node_wallet_json).chain), default_chain_config_json);
  test_node_config_json["chain_config"] = default_chain_config_json;
  ASSERT_EQ(enc_json(FullNodeConfig(test_node_config_json, test_node_wallet_json).chain), default_chain_config_json);
  test_node_config_json["chain_config"] = "test";
  ASSERT_EQ(enc_json(FullNodeConfig(test_node_config_json, test_node_wallet_json).chain),
            enc_json(ChainConfig::predefined("test")));
}

}  // namespace taraxa::core_tests

int main(int argc, char **argv) {
  taraxa::static_init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
