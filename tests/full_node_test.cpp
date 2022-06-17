
#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "cli/config.hpp"
#include "cli/tools.hpp"
#include "common/static_init.hpp"
#include "dag/dag.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/rpc/Taraxa.h"
#include "node/node.hpp"
#include "pbft/pbft_manager.hpp"
#include "string"
#include "transaction/transaction_manager.hpp"
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

void send_dummy_trx() {
  std::string dummy_trx =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": 0,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 2004,
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7782 > /dev/null)";

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
  std::set<blk_hash_t> s1, s2;
  s1.emplace(blk1.getHash());
  s1.emplace(blk2.getHash());
  s2.emplace(blk3.getHash());
  EXPECT_EQ(db.getBlocksByLevel(1), s1);
  EXPECT_EQ(db.getBlocksByLevel(2), s2);

  // Transaction
  db.saveTransaction(*g_trx_signed_samples[0]);
  db.saveTransaction(*g_trx_signed_samples[1]);
  auto batch = db.createWriteBatch();
  db.addTransactionToBatch(*g_trx_signed_samples[2], batch);
  db.addTransactionToBatch(*g_trx_signed_samples[3], batch);
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[0]->getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[1]->getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[2]->getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[3]->getHash()));
  EXPECT_EQ(*g_trx_signed_samples[0], *db.getTransaction(g_trx_signed_samples[0]->getHash()));
  EXPECT_EQ(*g_trx_signed_samples[1], *db.getTransaction(g_trx_signed_samples[1]->getHash()));
  EXPECT_EQ(*g_trx_signed_samples[2], *db.getTransaction(g_trx_signed_samples[2]->getHash()));
  EXPECT_EQ(*g_trx_signed_samples[3], *db.getTransaction(g_trx_signed_samples[3]->getHash()));

  // PBFT manager previous round status
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold), 0);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod), 0);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount), 0);
  size_t previous_round_sortition_threshold = 2;
  db.savePbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold,
                                    previous_round_sortition_threshold);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold),
            previous_round_sortition_threshold);
  uint64_t previous_round_dpos_period = 3;
  db.savePbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, previous_round_dpos_period);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod),
            previous_round_dpos_period);
  size_t previous_round_dpos_total_votes_count = 4;
  db.savePbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount,
                                    previous_round_dpos_total_votes_count);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount),
            previous_round_dpos_total_votes_count);
  previous_round_sortition_threshold = 6;
  previous_round_dpos_period = 7;
  previous_round_dpos_total_votes_count = 8;
  batch = db.createWriteBatch();
  db.addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold,
                                   previous_round_sortition_threshold, batch);
  db.addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, previous_round_dpos_period,
                                   batch);
  db.addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount,
                                   previous_round_dpos_total_votes_count, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold),
            previous_round_sortition_threshold);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod),
            previous_round_dpos_period);
  EXPECT_EQ(db.getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount),
            previous_round_dpos_total_votes_count);

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
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::ExecutedBlock));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::ExecutedInRound));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash));
  db.savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  db.savePbftMgrStatus(PbftMgrStatus::ExecutedInRound, true);
  db.savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
  db.savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::ExecutedBlock));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::ExecutedInRound));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue));
  EXPECT_TRUE(db.getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash));
  batch = db.createWriteBatch();
  db.addPbftMgrStatusToBatch(PbftMgrStatus::ExecutedBlock, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::ExecutedInRound, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  db.addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db.commitWriteBatch(batch);
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::ExecutedBlock));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::ExecutedInRound));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue));
  EXPECT_FALSE(db.getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash));

  // PBFT manager voted value
  EXPECT_EQ(db.getPbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound), nullptr);
  EXPECT_EQ(db.getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound), nullptr);
  EXPECT_EQ(db.getPbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue), nullptr);
  db.savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, blk_hash_t(1));
  db.savePbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound, blk_hash_t(2));
  db.savePbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue, blk_hash_t(3));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound), blk_hash_t(1));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound), blk_hash_t(2));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue), blk_hash_t(3));
  batch = db.createWriteBatch();
  db.addPbftMgrVotedValueToBatch(PbftMgrVotedValue::OwnStartingValueInRound, blk_hash_t(4), batch);
  db.addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound, blk_hash_t(5), batch);
  db.addPbftMgrVotedValueToBatch(PbftMgrVotedValue::LastCertVotedValue, blk_hash_t(6), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound), blk_hash_t(4));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound), blk_hash_t(5));
  EXPECT_EQ(*db.getPbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue), blk_hash_t(6));

  // PBFT cert voted block
  auto pbft_block1 = make_simple_pbft_block(blk_hash_t(1), 1);
  auto pbft_block2 = make_simple_pbft_block(blk_hash_t(2), 2);
  auto pbft_block3 = make_simple_pbft_block(blk_hash_t(3), 3);
  auto pbft_block4 = make_simple_pbft_block(blk_hash_t(4), 4);
  EXPECT_EQ(db.getPbftCertVotedBlock(blk_hash_t(1)), nullptr);
  db.savePbftCertVotedBlock(*pbft_block1);
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block1->getBlockHash())->rlp(false), pbft_block1->rlp(false));
  batch = db.createWriteBatch();
  db.addPbftCertVotedBlockToBatch(*pbft_block2, batch);
  db.addPbftCertVotedBlockToBatch(*pbft_block3, batch);
  db.addPbftCertVotedBlockToBatch(*pbft_block4, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block2->getBlockHash())->rlp(false), pbft_block2->rlp(false));
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block3->getBlockHash())->rlp(false), pbft_block3->rlp(false));
  EXPECT_EQ(db.getPbftCertVotedBlock(pbft_block4->getBlockHash())->rlp(false), pbft_block4->rlp(false));

  // pbft_blocks and cert votes
  EXPECT_FALSE(db.pbftBlockInDb(blk_hash_t(0)));
  EXPECT_FALSE(db.pbftBlockInDb(blk_hash_t(1)));
  pbft_block1 = make_simple_pbft_block(blk_hash_t(1), 2);
  pbft_block2 = make_simple_pbft_block(blk_hash_t(2), 3);
  pbft_block3 = make_simple_pbft_block(blk_hash_t(3), 4);
  pbft_block4 = make_simple_pbft_block(blk_hash_t(4), 5);

  // Certified votes
  std::vector<std::shared_ptr<Vote>> cert_votes;
  for (auto i = 0; i < 3; i++) {
    VrfPbftMsg msg(cert_vote_type, 2, 3);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, blk_hash_t(10));
    cert_votes.emplace_back(std::make_shared<Vote>(vote));
  }

  batch = db.createWriteBatch();
  std::vector<std::shared_ptr<Vote>> votes;

  PeriodData period_data1(pbft_block1, cert_votes);
  PeriodData period_data2(pbft_block2, votes);
  PeriodData period_data3(pbft_block3, votes);
  PeriodData period_data4(pbft_block4, votes);

  db.savePeriodData(period_data1, batch);
  db.savePeriodData(period_data2, batch);
  db.savePeriodData(period_data3, batch);
  db.savePeriodData(period_data4, batch);

  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block1->getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block2->getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block3->getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block4->getBlockHash()));
  EXPECT_EQ(db.getPbftBlock(pbft_block1->getBlockHash())->rlp(false), pbft_block1->rlp(false));
  EXPECT_EQ(db.getPbftBlock(pbft_block2->getBlockHash())->rlp(false), pbft_block2->rlp(false));
  EXPECT_EQ(db.getPbftBlock(pbft_block3->getBlockHash())->rlp(false), pbft_block3->rlp(false));
  EXPECT_EQ(db.getPbftBlock(pbft_block4->getBlockHash())->rlp(false), pbft_block4->rlp(false));

  PeriodData pbft_block_cert_votes(pbft_block1, cert_votes);
  auto cert_votes_from_db = db.getCertVotes(pbft_block1->getPeriod());
  PeriodData pbft_block_cert_votes_from_db(pbft_block1, cert_votes_from_db);
  EXPECT_EQ(pbft_block_cert_votes.rlp(), pbft_block_cert_votes_from_db.rlp());

  // pbft_blocks (head)
  PbftChain pbft_chain(addr_t(), db_ptr);
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

  // Verified votes
  auto verified_votes = db.getVerifiedVotes();
  EXPECT_TRUE(verified_votes.empty());
  auto voted_pbft_block_hash = blk_hash_t(2);
  for (auto i = 0; i < 3; i++) {
    auto round = i;
    auto step = i;
    VrfPbftMsg msg(next_vote_type, round, step);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    verified_votes.emplace_back(std::make_shared<Vote>(vote));
  }
  db.saveVerifiedVote(verified_votes[0]);
  EXPECT_EQ(db.getVerifiedVote(verified_votes[0]->getHash())->rlp(true), verified_votes[0]->rlp(true));
  batch = db.createWriteBatch();
  db.addVerifiedVoteToBatch(verified_votes[1], batch);
  db.addVerifiedVoteToBatch(verified_votes[2], batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getVerifiedVotes().size(), verified_votes.size());
  batch = db.createWriteBatch();
  for (size_t i = 0; i < verified_votes.size(); i++) {
    db.removeVerifiedVoteToBatch(verified_votes[i]->getHash(), batch);
  }
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.getVerifiedVotes().empty());

  // Soft votes
  auto round = 1, step = 2;
  auto soft_votes = db.getSoftVotes(round);
  EXPECT_TRUE(soft_votes.empty());
  for (auto i = 0; i < 3; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(soft_vote_type, round, step);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    soft_votes.emplace_back(std::make_shared<Vote>(vote));
  }
  db.saveSoftVotes(round, soft_votes);
  auto soft_votes_from_db = db.getSoftVotes(round);
  EXPECT_EQ(soft_votes.size(), soft_votes_from_db.size());
  EXPECT_EQ(soft_votes_from_db.size(), 3);
  for (auto i = 3; i < 5; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(soft_vote_type, round, step);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    soft_votes.emplace_back(std::make_shared<Vote>(vote));
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

  // Next votes
  round = 3, step = 5;
  auto next_votes = db.getNextVotes(round);
  EXPECT_TRUE(next_votes.empty());
  for (auto i = 0; i < 3; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(next_vote_type, round, step);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    next_votes.emplace_back(std::make_shared<Vote>(vote));
  }
  db.saveNextVotes(round, next_votes);
  auto next_votes_from_db = db.getNextVotes(round);
  EXPECT_EQ(next_votes.size(), next_votes_from_db.size());
  EXPECT_EQ(next_votes_from_db.size(), 3);
  next_votes.clear();
  for (auto i = 3; i < 5; i++) {
    blk_hash_t voted_pbft_block_hash(i);
    VrfPbftMsg msg(next_vote_type, round, step);
    vrf_wrapper::vrf_sk_t vrf_sk(
        "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
        "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
    VrfPbftSortition vrf_sortition(vrf_sk, msg);
    Vote vote(g_secret, vrf_sortition, voted_pbft_block_hash);
    next_votes.emplace_back(std::make_shared<Vote>(vote));
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
  db.addPbftBlockPeriodToBatch(3, blk_hash_t(1), batch);
  db.addPbftBlockPeriodToBatch(4, blk_hash_t(2), batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(db.getPeriodFromPbftHash(blk_hash_t(1)).second, 3);
  EXPECT_EQ(db.getPeriodFromPbftHash(blk_hash_t(2)).second, 4);

  // dag_block_period
  batch = db.createWriteBatch();
  db.addDagBlockPeriodToBatch(blk_hash_t(1), 1, 2, batch);
  db.addDagBlockPeriodToBatch(blk_hash_t(2), 3, 4, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(1, db.getDagBlockPeriod(blk_hash_t(1))->first);
  EXPECT_EQ(2, db.getDagBlockPeriod(blk_hash_t(1))->second);
  EXPECT_EQ(3, db.getDagBlockPeriod(blk_hash_t(2))->first);
  EXPECT_EQ(4, db.getDagBlockPeriod(blk_hash_t(2))->second);

  // DPOS proposal period DAG levels map
  db.saveProposalPeriodDagLevelsMap(100, 0);
  db.saveProposalPeriodDagLevelsMap(103, 1);
  db.saveProposalPeriodDagLevelsMap(106, 3);
  EXPECT_TRUE(db.getProposalPeriodForDagLevel(5));
  EXPECT_TRUE(db.getProposalPeriodForDagLevel(100));
  EXPECT_EQ(*db.getProposalPeriodForDagLevel(5), 0);
  EXPECT_EQ(*db.getProposalPeriodForDagLevel(100), 0);

  EXPECT_TRUE(db.getProposalPeriodForDagLevel(101));
  EXPECT_TRUE(db.getProposalPeriodForDagLevel(103));
  EXPECT_EQ(*db.getProposalPeriodForDagLevel(101), 1);
  EXPECT_EQ(*db.getProposalPeriodForDagLevel(103), 1);

  EXPECT_TRUE(db.getProposalPeriodForDagLevel(105));
  EXPECT_TRUE(db.getProposalPeriodForDagLevel(106));
  EXPECT_EQ(*db.getProposalPeriodForDagLevel(105), 3);
  EXPECT_EQ(*db.getProposalPeriodForDagLevel(106), 3);
  EXPECT_FALSE(db.getProposalPeriodForDagLevel(107));
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
    std::unordered_set<trx_hash_t> transactions;

   public:
    context(decltype(nodes_) nodes) : nodes_(nodes) {
      for (auto const &node : nodes_) {
        trx_clients.emplace_back(node);
        expected_balances[node->getAddress()] = node->getFinalChain()->getBalance(node->getAddress()).first;
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
      EXPECT_NE(result.stage, TransactionClient::TransactionStage::created);
      transactions.emplace(result.trx->getHash());
    }

    void coin_transfer(int sender_node_i, addr_t const &to, val_t const &amount, bool verify_executed = true) {
      {
        unique_lock l(m);
        ++issued_trx_count;
        expected_balances[to] += amount;
        expected_balances[nodes_[sender_node_i]->getAddress()] -= amount;
      }
      auto result = trx_clients[sender_node_i].coinTransfer(to, amount, {}, verify_executed);
      EXPECT_NE(result.stage, TransactionClient::TransactionStage::created);
      transactions.emplace(result.trx->getHash());
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

    void assert_all_transactions_known() {
      for (auto &n : nodes_) {
        for (auto &t : transactions) {
          auto location = n->getFinalChain()->transaction_location(t);
          ASSERT_EQ(location.has_value(), true);
        }
      }
    }

    void wait_all_transactions_known(wait_opts const &wait_for = {30s, 500ms}) {
      wait(wait_for, [this](auto &ctx) {
        for (auto &n : nodes_) {
          for (auto &t : transactions) {
            if (!n->getFinalChain()->transaction_location(t)) {
              ctx.fail();
            }
          }
        }
        dummy_transaction();
      });
    }

  } context(nodes);

  std::vector<trx_hash_t> all_transactions;
  // transfer some coins to your friends ...
  auto init_bal = own_effective_genesis_bal(nodes[0]->getConfig()) / nodes.size();

  {
    for (size_t i(1); i < nodes.size(); ++i) {
      // we shouldn't wait for transaction execution because it could be in alternative dag
      context.coin_transfer(0, nodes[i]->getAddress(), init_bal, false);
    }
    context.wait_all_transactions_known();
  }

  std::cout << "Waiting until transactions are executed" << std::endl;
  const auto trx_count = context.getIssuedTrxCount();
  EXPECT_HAPPENS({60s, 200ms}, [&](auto &ctx) {
    for (size_t i = 0; i < nodes.size(); ++i)
      WAIT_EXPECT_EQ(ctx, nodes[i]->getDB()->getNumTransactionExecuted(), trx_count)
  });

  std::cout << "Initial coin transfers from node 0 issued ... " << std::endl;

  {
    for (size_t i(0); i < nodes.size(); ++i) {
      auto to = i < nodes.size() - 1 ? nodes[i + 1]->getAddress() : addr_t("d79b2575d932235d87ea2a08387ae489c31aa2c9");
      for (auto _(0); _ < 10; ++_) {
        // we shouldn't wait for transaction execution because it could be in alternative dag
        context.coin_transfer(i, to, 100, false);
      }
    }
    context.wait_all_transactions_known();
  }
  std::cout << "Issued " << context.getIssuedTrxCount() << " transactions" << std::endl;

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
                << " Node 5: Dag size = " << num_vertices5.first << " Trx count = " << num_trx5 << std::endl
                << " Issued transaction count = " << issued_trx_count << std::endl;
      std::cout << "Send a dummy transaction to coverge DAG" << std::endl;
      context.dummy_transaction();
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  auto num_proposed_blocks = nodes[0]->getNumProposedBlocks();
  // wait for next block
  wait({20s, 500ms}, [&](auto &ctx) {
    if (nodes[0]->getNumProposedBlocks() == num_proposed_blocks + 1) ctx.fail();
    if (nodes[1]->getNumProposedBlocks() == num_proposed_blocks + 1) ctx.fail();
    if (nodes[2]->getNumProposedBlocks() == num_proposed_blocks + 1) ctx.fail();
    if (nodes[3]->getNumProposedBlocks() == num_proposed_blocks + 1) ctx.fail();
    if (nodes[4]->getNumProposedBlocks() == num_proposed_blocks + 1) ctx.fail();
  });

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
    issued_trx_count = context.getIssuedTrxCount();
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
    issued_trx_count = context.getIssuedTrxCount();
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
    issued_trx_count = context.getIssuedTrxCount();
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

  context.assert_all_transactions_known();
  context.assert_balances_synced();
}

TEST_F(FullNodeTest, insert_anchor_and_compute_order) {
  auto node_cfgs = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfgs);
  auto &node = nodes[0];

  auto mock_dags = samples::createMockDag1(node->getConfig().chain.dag_genesis_block.getHash().toString());

  for (int i = 1; i <= 9; i++) {
    node->getDagManager()->addDagBlock(std::move(mock_dags[i]));
  }
  // -------- first period ----------

  auto ret = node->getDagManager()->getLatestPivotAndTips();
  uint64_t period = 1;
  vec_blk_t order;
  order = node->getDagManager()->getDagBlockOrder(ret->first, period);
  EXPECT_EQ(order.size(), 6);

  if (order.size() == 6) {
    EXPECT_EQ(order[0], blk_hash_t(3));
    EXPECT_EQ(order[1], blk_hash_t(6));
    EXPECT_EQ(order[2], blk_hash_t(2));
    EXPECT_EQ(order[3], blk_hash_t(1));
    EXPECT_EQ(order[4], blk_hash_t(5));
    EXPECT_EQ(order[5], blk_hash_t(7));
  }
  {
    std::unique_lock dag_lock(node->getDagManager()->getDagMutex());
    auto num_blks_set = node->getDagManager()->setDagBlockOrder(ret->first, period, order);
    EXPECT_EQ(num_blks_set, 6);
  }
  // -------- second period ----------

  for (int i = 10; i <= 16; i++) {
    node->getDagManager()->addDagBlock(std::move(mock_dags[i]));
  }

  ret = node->getDagManager()->getLatestPivotAndTips();
  period = 2;
  order = node->getDagManager()->getDagBlockOrder(ret->first, period);
  if (order.size() == 7) {
    EXPECT_EQ(order[0], blk_hash_t(11));
    EXPECT_EQ(order[1], blk_hash_t(10));
    EXPECT_EQ(order[2], blk_hash_t(13));
    EXPECT_EQ(order[3], blk_hash_t(9));
    EXPECT_EQ(order[4], blk_hash_t(12));
    EXPECT_EQ(order[5], blk_hash_t(14));
    EXPECT_EQ(order[6], blk_hash_t(15));
  }
  {
    std::unique_lock dag_lock(node->getDagManager()->getDagMutex());
    auto num_blks_set = node->getDagManager()->setDagBlockOrder(ret->first, period, order);
    EXPECT_EQ(num_blks_set, 7);
  }

  // -------- third period ----------

  for (size_t i = 17; i < mock_dags.size(); i++) {
    node->getDagManager()->addDagBlock(std::move(mock_dags[i]));
  }

  ret = node->getDagManager()->getLatestPivotAndTips();
  period = 3;
  order = node->getDagManager()->getDagBlockOrder(ret->first, period);
  if (order.size() == 5) {
    EXPECT_EQ(order[0], blk_hash_t(17));
    EXPECT_EQ(order[1], blk_hash_t(16));
    EXPECT_EQ(order[2], blk_hash_t(8));
    EXPECT_EQ(order[3], blk_hash_t(18));
    EXPECT_EQ(order[4], blk_hash_t(19));
  }
  {
    std::unique_lock dag_lock(node->getDagManager()->getDagMutex());
    auto num_blks_set = node->getDagManager()->setDagBlockOrder(ret->first, period, order);
    EXPECT_EQ(num_blks_set, 5);
  }
}

TEST_F(FullNodeTest, destroy_db) {
  auto node_cfgs = make_node_cfgs(1);
  {
    auto node = create_nodes(node_cfgs).front();
    auto db = node->getDB();
    db->saveTransaction(*g_trx_signed_samples[0]);
    // Verify trx saved in db
    EXPECT_TRUE(db->getTransaction(g_trx_signed_samples[0]->getHash()));
  }
  {
    auto node = create_nodes(node_cfgs).front();
    auto db = node->getDB();
    // Verify trx saved in db after restart with destroy_db false
    EXPECT_TRUE(db->getTransaction(g_trx_signed_samples[0]->getHash()));
  }
}

TEST_F(FullNodeTest, reconstruct_anchors) {
  auto node_cfgs = make_node_cfgs<5>(1);
  std::pair<blk_hash_t, blk_hash_t> anchors;
  {
    auto node = create_nodes(node_cfgs, true /*start*/).front();

    taraxa::thisThreadSleepForMilliSeconds(500);

    TransactionClient trx_client(node);

    for (auto i = 0; i < 3; i++) {
      auto result = trx_client.coinTransfer(KeyPair::create().address(), 10, {}, false);
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
    anchors = node->getDagManager()->getAnchors();
  }
  {
    auto node = create_nodes(node_cfgs, true /*start*/).front();

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

  auto mock_dags = samples::createMockDag0(node_cfgs.front().chain.dag_genesis_block.getHash().toString());
  auto num_blks = mock_dags.size();

  {
    auto node = create_nodes(node_cfgs, true /*start*/).front();

    taraxa::thisThreadSleepForMilliSeconds(100);

    for (size_t i = 1; i < num_blks; i++) {
      node->getDagManager()->addDagBlock(DagBlock(mock_dags[i]));
    }

    taraxa::thisThreadSleepForMilliSeconds(100);
    vertices1 = node->getDagManager()->getNumVerticesInDag().first;
    EXPECT_EQ(vertices1, num_blks);
  }
  {
    auto node = create_nodes(node_cfgs, true /*start*/).front();
    taraxa::thisThreadSleepForMilliSeconds(100);

    vertices2 = node->getDagManager()->getNumVerticesInDag().first;
    EXPECT_EQ(vertices2, num_blks);
  }
  {
    fs::remove_all(node_cfgs[0].db_path);
    auto node = create_nodes(node_cfgs, true /*start*/).front();
    // TODO: pbft does not support node stop yet, to be fixed ...
    node->getPbftManager()->stop();
    for (size_t i = 1; i < num_blks; i++) {
      node->getDagManager()->addDagBlock(std::move(mock_dags[i]));
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
    vertices3 = node->getDagManager()->getNumVerticesInDag().first;
  }
  {
    auto node = create_nodes(node_cfgs, true /*start*/).front();
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
  for (const auto &trx : samples::createSignedTrxSamples(0, 400, g_secret)) {
    nodes[0]->getTransactionManager()->insertTransaction(trx);
  }
  for (const auto &trx : samples::createSignedTrxSamples(400, 1000, g_secret)) {
    nodes[1]->getTransactionManager()->insertTransaction(trx);
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
    if (nodes[0]->getTransactionManager()->getTransactionPoolSize() == 0 &&
        nodes[1]->getTransactionManager()->getTransactionPoolSize() == 0 && num_vertices1 == num_vertices2)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_vertices1 = nodes[0]->getDagManager()->getNumVerticesInDag();
    num_vertices2 = nodes[1]->getDagManager()->getNumVerticesInDag();
  }
  EXPECT_GE(num_vertices1.first, 2);
  EXPECT_EQ(num_vertices1, num_vertices2);
}

TEST_F(FullNodeTest, persist_counter) {
  auto node_cfgs = make_node_cfgs<2, true>(2);
  unsigned long num_exe_trx1 = 0, num_exe_trx2 = 0, num_exe_blk1 = 0, num_exe_blk2 = 0, num_trx1 = 0, num_trx2 = 0;
  {
    auto nodes = launch_nodes(node_cfgs);

    // send 1000 trxs
    for (const auto &trx : samples::createSignedTrxSamples(0, 400, g_secret)) {
      nodes[0]->getTransactionManager()->insertTransaction(trx);
    }
    for (const auto &trx : samples::createSignedTrxSamples(400, 1000, g_secret)) {
      nodes[1]->getTransactionManager()->insertTransaction(trx);
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
    sendTrx(1000, 7782);
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
  auto node = create_nodes(node_cfgs, true /*start*/).front();

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf868808502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba07659e8c7207a4b2cd96488108fed54c463b1719b438add1159beed04f6660da8a028feb0a3b44bd34e0dd608f82aeeb2cd70d1305653b5dc33678be2ffcfcac997"
                                      ]}' 0.0.0.0:7782)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf868018502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba05256492dd60623ab5a403ed1b508f845f87f631d2c2e7acd4357cd83ef5b6540a042def7cd4f3c25ce67ee25911740dab47161e096f1dd024badecec58888a890b"
                                      ]}' 0.0.0.0:7782)";

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
  // Will be rejected same nonce
  EXPECT_FALSE(system(send_raw_trx2.c_str()));

  EXPECT_HAPPENS({60s, 1s}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), 1)
    WAIT_EXPECT_EQ(ctx, node->getTransactionManager()->getTransactionCount(), 1)
    WAIT_EXPECT_EQ(ctx, node->getDagManager()->getNumVerticesInDag().first, 2)
  });
}

TEST_F(FullNodeTest, two_nodes_run_two_transactions) {
  auto node_cfgs = make_node_cfgs<5, true>(2);
  auto nodes = launch_nodes(node_cfgs);

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf86b808502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca04cef8610e05b4476673c369204899da747a9b32b0ad3769814b620281c900408a0130499a83d0b56c184c6ac518f6bbe2f8f946b65bf42b08cfc9b4dfbe2ebfd04"
                                      ]}' 0.0.0.0:7782)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf86b018502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca06644c30a23286d0de8fa107f1bded3a7a214004042b2007b2a9a62c8b313cf79a06cbb522856838b107542d8213286928500b2822584d6c7c6fee3a2c348cade4a"
                                      ]}' 0.0.0.0:7782)";

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
  // Will be rejected same nonce
  EXPECT_FALSE(system(send_raw_trx2.c_str()));

  trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();

  for (unsigned i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = nodes[0]->getDB()->getNumTransactionExecuted();
    if (trx_executed1 == 1) break;
    thisThreadSleepForMilliSeconds(1000);
  }
  EXPECT_EQ(nodes[0]->getTransactionManager()->getTransactionCount(), 1);
  EXPECT_GE(nodes[0]->getDagManager()->getNumVerticesInDag().first, 2);
  EXPECT_EQ(trx_executed1, 1);
}

TEST_F(FullNodeTest, save_network_to_file) {
  auto node_cfgs = make_node_cfgs(3);
  // Create and destroy to create network. So next time will be loaded from file
  { auto nodes = launch_nodes(node_cfgs); }
  {
    auto nodes = create_nodes({node_cfgs[1], node_cfgs[2]}, true /*start*/);

    for (unsigned i = 0; i < SYNC_TIMEOUT; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (1 == nodes[0]->getNetwork()->getPeerCount() && 1 == nodes[1]->getNetwork()->getPeerCount()) break;
    }

    ASSERT_EQ(1, nodes[0]->getNetwork()->getPeerCount());
    ASSERT_EQ(1, nodes[1]->getNetwork()->getPeerCount());
  }
}

TEST_F(FullNodeTest, receive_send_transaction) {
  auto node_cfgs = make_node_cfgs<20, true>(1);
  auto node = create_nodes(node_cfgs, true /*start*/).front();

  try {
    sendTrx(1000, 7782);
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
  auto nodes = launch_nodes(node_cfgs);
  const auto expected_balances = effective_genesis_balances(node_cfgs[0].chain.final_chain.state);
  const auto node_1_genesis_bal = own_effective_genesis_bal(node_cfgs[0]);

  // Even distribute coins from master boot node to other nodes. Since master
  // boot node owns whole coins, the active players should be only master boot
  // node at the moment.
  const auto gas_price = 0;
  auto nonce = 1;
  uint64_t trxs_count = 0;
  auto test_transfer_val = node_1_genesis_bal / node_cfgs.size();
  for (size_t i(1); i < nodes.size(); ++i) {
    auto master_boot_node_send_coins = std::make_shared<Transaction>(
        nonce++, test_transfer_val, gas_price, 100000, bytes(), nodes[0]->getSecretKey(), nodes[i]->getAddress());
    // broadcast trx and insert
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins);
    trxs_count++;
  }

  std::cout << "Checking all nodes executed transactions at initialization" << std::endl;
  wait({150s, 2s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions, expected " << trxs_count << std::endl;
        if (ctx.fail(); !ctx.is_last_attempt) {
          auto dummy_trx = std::make_shared<Transaction>(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(),
                                                         nodes[0]->getAddress());
          // broadcast dummy transaction
          nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first,
                expected_balances.at(nodes[j]->getAddress()) + test_transfer_val);
    }
  }

  // Sending coins in Robin Cycle
  auto send_coins = 1;
  for (size_t i(0); i < nodes.size(); ++i) {
    auto receiver_index = (i + 1) % nodes.size();
    // Each node sends 500 transactions
    auto j = 0;
    for (; j < 500; j++) {
      auto send_coins_in_robin_cycle =
          std::make_shared<Transaction>(nonce++, send_coins, gas_price, 100000, bytes(), nodes[i]->getSecretKey(),
                                        nodes[receiver_index]->getAddress());
      // broadcast trx and insert
      nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle);
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
          auto dummy_trx = std::make_shared<Transaction>(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(),
                                                         nodes[0]->getAddress());
          // broadcast dummy transaction
          nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
  auto trxs_table = nodes[0]->getDB()->getAllTransactionPeriod();
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
  uint64_t trxs_count_at_pbft_size_5 = 0;
  uint64_t executed_trxs = 0;
  uint64_t executed_chain_size = 0;

  {
    auto node_cfgs = make_node_cfgs<5>(1);
    auto nodes = launch_nodes(node_cfgs);

    auto gas_price = val_t(2);
    auto data = bytes();
    auto nonce = 0;

    // Issue dummy trx until at least 10 pbft blocks created
    while (executed_chain_size < 10) {
      auto dummy_trx = std::make_shared<Transaction>(nonce++, 0, gas_price, TEST_TX_GAS_LIMIT, bytes(),
                                                     nodes[0]->getSecretKey(), nodes[0]->getAddress());
      nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
    node_cfgs[0].db_config.rebuild_db = true;
    auto nodes = launch_nodes(node_cfgs);
  }

  {
    std::cout << "Check rebuild DB" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    auto nodes = launch_nodes(node_cfgs);
    EXPECT_HAPPENS({10s, 100ms}, [&](auto &ctx) {
      WAIT_EXPECT_EQ(ctx, nodes[0]->getDB()->getNumTransactionExecuted(), trxs_count)
      WAIT_EXPECT_EQ(ctx, nodes[0]->getFinalChain()->last_block_number(), executed_chain_size)
    });
  }

  {
    std::cout << "Test rebuild for period 5" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    node_cfgs[0].db_config.rebuild_db = true;
    node_cfgs[0].db_config.rebuild_db_period = 5;
    auto nodes = launch_nodes(node_cfgs);
  }

  {
    std::cout << "Check rebuild for period 5" << std::endl;
    auto node_cfgs = make_node_cfgs<5>(1);
    auto nodes = launch_nodes(node_cfgs);
    EXPECT_HAPPENS({10s, 100ms}, [&](auto &ctx) {
      WAIT_EXPECT_EQ(ctx, nodes[0]->getDB()->getNumTransactionExecuted(), trxs_count_at_pbft_size_5)
      WAIT_EXPECT_EQ(ctx, nodes[0]->getFinalChain()->last_block_number(), 5)
    });
  }
}

TEST_F(FullNodeTest, transfer_to_self) {
  auto node_cfgs = make_node_cfgs<5, true>(1);
  auto nodes = launch_nodes(node_cfgs);

  std::cout << "Send first trx ..." << std::endl;
  auto node_addr = nodes[0]->getAddress();
  auto initial_bal = nodes[0]->getFinalChain()->getBalance(node_addr);
  uint64_t trx_count(100);
  EXPECT_TRUE(initial_bal.second);
  for (uint64_t i = 0; i < trx_count; ++i) {
    const auto trx = std::make_shared<Transaction>(i, i * 100, 0, 1000000, str2bytes("00FEDCBA9876543210000000"),
                                                   g_secret, node_addr);
    nodes[0]->getTransactionManager()->insertTransaction(trx);
  }
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
  const string expected_default_chain_cfg_json = R"({
  "dag_genesis_block": {
    "level": "0x0",
    "pivot": "0x0000000000000000000000000000000000000000000000000000000000000000",
    "sig": "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
    "timestamp": "0x5d422b80",
    "tips": [],
    "transactions": [],
    "trx_estimations" : []
  },
  "final_chain": {
    "genesis_block_fields": {
      "author": "0x0000000000000000000000000000000000000000",
      "timestamp": "0x0"
    },
    "state": {
      "dpos": {
        "delegation_delay": "0x0",
        "delegation_locking_period": "0x0",
        "eligibility_balance_threshold": "0x3b9aca00",
        "vote_eligibility_balance_step": "0x3b9aca00",
        "validator_maximum_stake":"0x84595161401484a000000",
        "minimum_deposit":"0x0",
        "commission_change_delta":"0x0",
        "commission_change_frequency":"0x0",
        "yield_percentage":"0x14",
        "blocks_per_year":"0x3c2670",
        "initial_validators": []
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
        "disable_nonce_check": false,
        "enable_nonce_skipping": false
      },
      "block_rewards_options": {
        "disable_block_rewards": true,
        "disable_contract_distribution": true
      },
      "genesis_balances": {
      }
    }
  },
  "gas_price": {
    "blocks": 200,
    "percentile": 60
  },
  "pbft": {
    "committee_size": "0x5",
    "dag_blocks_size": "0x64",
    "ghost_path_move_back": "0x1",
    "lambda_ms_min": "0x7d0",
    "number_of_proposers" : "0x14",
    "gas_limit": "0x3938700"
  },
  "dag": {
    "block_proposer": {
      "shard": "0x1",
      "transaction_limit": "0xfa"
    },
    "gas_limit": "0x989680"
  },
  "sortition": {
      "changes_count_for_average": 10,
      "dag_efficiency_targets": [6900, 7100],
      "changing_interval": 200,
      "computation_interval": 50,
      "vrf": {
        "threshold_upper": "0xafff",
        "threshold_range": 80
      },
      "vdf": {
        "difficulty_max": 21,
        "difficulty_min": 16,
        "difficulty_stale": 23,
        "lambda_bound": "0x64"
      }
    }
})";
  Json::Value default_chain_config_json;
  std::istringstream(expected_default_chain_cfg_json) >> default_chain_config_json;
  // TODO [1473] : remove jsonToUnstyledString
  std::string config_file_path = DIR_CONF / "conf_taraxa1.json";
  Json::Value test_node_config_json;
  std::ifstream(config_file_path, std::ifstream::binary) >> test_node_config_json;
  Json::Value test_node_wallet_json;
  std::ifstream((DIR_CONF / "wallet1.json").string(), std::ifstream::binary) >> test_node_wallet_json;
  ASSERT_EQ(jsonToUnstyledString(
                enc_json(FullNodeConfig(test_node_config_json, test_node_wallet_json, config_file_path).chain)),
            jsonToUnstyledString(default_chain_config_json));
  test_node_config_json["chain_config"] = default_chain_config_json;
  ASSERT_EQ(jsonToUnstyledString(
                enc_json(FullNodeConfig(test_node_config_json, test_node_wallet_json, config_file_path).chain)),
            jsonToUnstyledString(default_chain_config_json));
}

TEST_F(FullNodeTest, transaction_validation) {
  auto node_cfgs = make_node_cfgs<5>(1);
  auto nodes = launch_nodes(node_cfgs);
  uint32_t nonce = 0;
  const uint32_t gasprice = 1;
  const uint32_t gas = 100000;

  auto trx = std::make_shared<Transaction>(nonce++, 0, gasprice, gas, str2bytes("00FEDCBA9876543210000000"),
                                           nodes[0]->getSecretKey(), addr_t::random());
  // PASS on GAS
  EXPECT_TRUE(nodes[0]->getTransactionManager()->insertTransaction(trx).first);
  trx =
      std::make_shared<Transaction>(nonce++, 0, gasprice, FinalChain::GAS_LIMIT + 1,
                                    str2bytes("00FEDCBA9876543210000000"), nodes[0]->getSecretKey(), addr_t::random());
  // FAIL on GAS
  EXPECT_FALSE(nodes[0]->getTransactionManager()->insertTransaction(trx).first);

  trx = std::make_shared<Transaction>(nonce++, 0, gasprice, gas, str2bytes("00FEDCBA9876543210000000"),
                                      nodes[0]->getSecretKey(), addr_t::random());
  // PASS on NONCE
  EXPECT_TRUE(nodes[0]->getTransactionManager()->insertTransaction(trx).first);
  wait({60s, 200ms}, [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, nodes[0]->getDB()->getNumTransactionExecuted(), 2) });
  trx = std::make_shared<Transaction>(0, 0, gasprice, gas, str2bytes("00FEDCBA9876543210000000"),
                                      nodes[0]->getSecretKey(), addr_t::random());
  // FAIL on NONCE
  EXPECT_FALSE(nodes[0]->getTransactionManager()->insertTransaction(trx).first);

  trx = std::make_shared<Transaction>(nonce++, 0, gasprice, gas, str2bytes("00FEDCBA9876543210000000"),
                                      nodes[0]->getSecretKey(), addr_t::random());
  // PASS on BALANCE
  EXPECT_TRUE(nodes[0]->getTransactionManager()->insertTransaction(trx).first);

  trx =
      std::make_shared<Transaction>(nonce++, own_effective_genesis_bal(nodes[0]->getConfig()) + 1, 0, gas,
                                    str2bytes("00FEDCBA9876543210000000"), nodes[0]->getSecretKey(), addr_t::random());
  // FAIL on BALANCE
  EXPECT_FALSE(nodes[0]->getTransactionManager()->insertTransaction(trx).first);
}

TEST_F(FullNodeTest, light_node) {
  auto node_cfgs = make_node_cfgs<10>(2);
  node_cfgs[0].is_light_node = true;
  node_cfgs[0].light_node_history = 10;
  node_cfgs[0].dag_expiry_limit = 5;
  node_cfgs[0].max_levels_per_period = 3;
  node_cfgs[1].dag_expiry_limit = 5;
  node_cfgs[1].max_levels_per_period = 3;
  auto nodes = launch_nodes(node_cfgs);
  uint64_t nonce = 0;
  while (nodes[1]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks() < 20) {
    auto dummy_trx =
        std::make_shared<Transaction>(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(), nodes[0]->getAddress());
    // broadcast dummy transaction
    nodes[1]->getTransactionManager()->insertTransaction(dummy_trx);
    thisThreadSleepForMilliSeconds(200);
  }
  EXPECT_HAPPENS({10s, 1s}, [&](auto &ctx) {
    // Verify full node and light node sync without any issues
    WAIT_EXPECT_EQ(ctx, nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                   nodes[1]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks())
  });
  uint32_t non_empty_counter = 0;
  for (uint64_t i = 0; i < nodes[0]->getPbftChain()->getPbftChainSize(); i++) {
    const auto pbft_block = nodes[0]->getDB()->getPbftBlock(i);
    if (pbft_block) {
      non_empty_counter++;
    }
  }
  // Verify light node stores only expected number of period_data
  EXPECT_GE(non_empty_counter, node_cfgs[0].light_node_history);
  EXPECT_LE(non_empty_counter, node_cfgs[0].light_node_history + 1);
}

TEST_F(FullNodeTest, clear_period_data) {
  auto node_cfgs = make_node_cfgs<10>(2);
  node_cfgs[1].is_light_node = true;
  node_cfgs[1].light_node_history = 4;
  node_cfgs[0].dag_expiry_limit = 15;
  node_cfgs[0].max_levels_per_period = 3;
  node_cfgs[1].dag_expiry_limit = 15;
  node_cfgs[1].max_levels_per_period = 3;
  auto nodes = launch_nodes(node_cfgs);
  uint64_t nonce = 0;
  size_t node1_chain_size = 0, node2_chain_size = 0;
  while (node2_chain_size < 20) {
    auto dummy_trx =
        std::make_shared<Transaction>(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(), nodes[0]->getAddress());
    // broadcast dummy transaction
    if (node1_chain_size == node2_chain_size) nodes[1]->getTransactionManager()->insertTransaction(dummy_trx);
    thisThreadSleepForMilliSeconds(500);
    node1_chain_size = nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks();
    node2_chain_size = nodes[1]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks();
  }
  EXPECT_HAPPENS({10s, 1s}, [&](auto &ctx) {
    // Verify full node and light node sync without any issues
    WAIT_EXPECT_EQ(ctx, nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                   nodes[1]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks())
  });
  uint32_t non_empty_counter = 0;
  uint64_t last_anchor_level;
  for (uint64_t i = 0; i < nodes[0]->getPbftChain()->getPbftChainSize(); i++) {
    const auto pbft_block = nodes[0]->getDB()->getPbftBlock(i);
    if (pbft_block && pbft_block->getPivotDagBlockHash() != NULL_BLOCK_HASH) {
      non_empty_counter++;
      last_anchor_level = nodes[0]->getDB()->getDagBlock(pbft_block->getPivotDagBlockHash())->getLevel();
    }
  }
  uint32_t first_over_limit = 0;
  for (uint64_t i = 0; i < nodes[1]->getPbftChain()->getPbftChainSize(); i++) {
    const auto pbft_block = nodes[1]->getDB()->getPbftBlock(i);
    if (pbft_block && pbft_block->getPivotDagBlockHash() != NULL_BLOCK_HASH) {
      if (nodes[1]->getDB()->getDagBlock(pbft_block->getPivotDagBlockHash())->getLevel() +
              node_cfgs[0].dag_expiry_limit >=
          last_anchor_level) {
        first_over_limit = i;
        break;
      }
    }
  }

  std::cout << "Non empty counter: " << non_empty_counter << std::endl;
  std::cout << "Last anchor level: " << last_anchor_level << std::endl;
  std::cout << "First over limit: " << first_over_limit << std::endl;

  // Verify light node does not delete non expired dag blocks
  EXPECT_TRUE(nodes[0]->getDB()->getPbftBlock(first_over_limit));
}

}  // namespace taraxa::core_tests

int main(int argc, char **argv) {
  taraxa::static_init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
