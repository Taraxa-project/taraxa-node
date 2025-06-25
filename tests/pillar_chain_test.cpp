#include <gtest/gtest.h>

#include "common/encoding_solidity.hpp"
#include "common/init.hpp"
#include "logger/logging.hpp"
#include "pbft/pbft_manager.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

struct PillarChainTest : NodesTest {};

TEST_F(PillarChainTest, pillar_chain_db) {
  auto db_ptr = std::make_shared<DbStorage>(data_dir);
  auto& db = *db_ptr;

  // Pillar chain
  EthBlockNumber pillar_block_period(123);
  h256 state_root(456);
  blk_hash_t previous_pillar_block_hash(789);

  std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange> votes_count_changes;

  const auto pillar_block = std::make_shared<pillar_chain::PillarBlock>(
      pillar_block_period, state_root, previous_pillar_block_hash, h256{}, 0, std::move(votes_count_changes));

  // Pillar block vote counts
  std::vector<state_api::ValidatorVoteCount> vote_counts;

  // Current pillar block data - block + vote counts
  pillar_chain::CurrentPillarBlockDataDb current_pillar_block_data{pillar_block, vote_counts};
  db.saveCurrentPillarBlockData(current_pillar_block_data);

  const auto current_pillar_block_data_db = db.getCurrentPillarBlockData();
  EXPECT_EQ(pillar_block->getHash(), current_pillar_block_data_db->pillar_block->getHash());
  EXPECT_EQ(vote_counts.size(), current_pillar_block_data_db->vote_counts.size());
  for (size_t idx = 0; idx < current_pillar_block_data_db->vote_counts.size(); idx++) {
    EXPECT_EQ(current_pillar_block_data_db->vote_counts[idx].addr, current_pillar_block_data_db->vote_counts[idx].addr);
    EXPECT_EQ(current_pillar_block_data_db->vote_counts[idx].vote_count,
              current_pillar_block_data_db->vote_counts[idx].vote_count);
  }

  // Pillar block

  const auto previous_pillar_block =
      std::make_shared<pillar_chain::PillarBlock>(pillar_block_period - 1, h256{}, blk_hash_t{}, h256{}, 0,
                                                  std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange>{});
  db.savePillarBlock(pillar_block);
  db.savePillarBlock(previous_pillar_block);

  const auto pillar_block_db = db.getPillarBlock(pillar_block->getPeriod());
  EXPECT_EQ(pillar_block->getHash(), pillar_block_db->getHash());

  const auto latest_pillar_block_db = db.getLatestPillarBlock();
  EXPECT_EQ(pillar_block->getHash(), latest_pillar_block_db->getHash());

  // Pillar vote
  auto pillar_vote =
      std::make_shared<PillarVote>(secret_t::random(), pillar_block->getPeriod(), pillar_block->getHash());
  db.saveOwnPillarBlockVote(pillar_vote);
  auto pillar_vote_db = db.getOwnPillarBlockVote();
  EXPECT_EQ(pillar_vote->getHash(), pillar_vote_db->getHash());
}

TEST_F(PillarChainTest, pillar_blocks_create) {
  auto node_cfgs = make_node_cfgs(2, 2, 10);
  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_blocks_interval = 4;
  }

  auto nodes = launch_nodes(node_cfgs);

  // Wait until nodes create at least 2 pillar blocks
  const auto pillar_blocks_count = 2;
  const auto min_amount_of_pbft_blocks =
      pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(), min_amount_of_pbft_blocks + 1)
    }
  });

  for (auto& node : nodes) {
    node->getPbftManager()->stop();

    // Check if right amount of pillar blocks were created
    const auto latest_pillar_block = node->getDB()->getLatestPillarBlock();
    ASSERT_TRUE(latest_pillar_block);
    ASSERT_EQ(latest_pillar_block->getPeriod(),
              pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval);
  }
}

TEST_F(PillarChainTest, votes_count_changes) {
  const auto validators_count = 5;
  auto node_cfgs = make_node_cfgs(validators_count, validators_count, 10);

  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_blocks_interval = 4;
    node_cfg.genesis.state.hardforks.soleirolia_hf.block_num = 0;
  }
  auto nodes = launch_nodes(node_cfgs);

  auto wait_for_next_pillar_block = [&](size_t txs_count) -> PbftPeriod {
    EXPECT_HAPPENS({20s, 100ms}, [&](auto& ctx) {
      for (auto& node : nodes) {
        if (ctx.fail_if(node->getDB()->getNumTransactionExecuted() != txs_count)) {
          return;
        }
      }
    });
    auto chain_size = nodes[0]->getPbftChain()->getPbftChainSize();

    // Wait until new pillar block with changed validators vote_counts is created
    auto new_pillar_block_period = chain_size -
                                   chain_size % node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval +
                                   node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval;
    EXPECT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
      for (const auto& node : nodes) {
        if (ctx.fail_if(node->getPbftChain()->getPbftChainSize() < new_pillar_block_period + 1)) {
          return;
        }
      }
    });

    return new_pillar_block_period;
  };

  auto checkPillarBlockData = [&](size_t pillar_block_period,
                                  std::unordered_map<addr_t, dev::s256> expected_validators_vote_counts_changes) {
    // Check if vote_counts changes in new pillar block changed according to new delegations
    for (auto& node : nodes) {
      // Check if right amount of pillar blocks were created
      const auto new_pillar_block = node->getDB()->getPillarBlock(pillar_block_period);
      ASSERT_TRUE(new_pillar_block);
      ASSERT_EQ(new_pillar_block->getPeriod(), pillar_block_period);
      ASSERT_EQ(new_pillar_block->getValidatorsVoteCountsChanges().size(),
                expected_validators_vote_counts_changes.size());
      for (const auto& vote_count_change : new_pillar_block->getValidatorsVoteCountsChanges()) {
        EXPECT_TRUE(expected_validators_vote_counts_changes.contains(vote_count_change.addr_));
        ASSERT_EQ(vote_count_change.vote_count_change_,
                  expected_validators_vote_counts_changes[vote_count_change.addr_]);
      }
    }
  };

  // Initial stakes of all validators
  std::unordered_map<addr_t, dev::s256> expected_validators_vote_counts_changes;
  for (const auto& validator : node_cfgs[0].genesis.state.dpos.initial_validators) {
    auto& vote_count = expected_validators_vote_counts_changes[validator.address];
    for (const auto& delegation : validator.delegations) {
      vote_count += delegation.second / node_cfgs[0].genesis.state.dpos.vote_eligibility_balance_step;
    }
  }

  // Wait until nodes create first pillar block
  const auto first_pillar_block_period = node_cfgs[0].genesis.state.hardforks.ficus_hf.firstPillarBlockPeriod();
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(), first_pillar_block_period + 1)
    }
  });

  checkPillarBlockData(first_pillar_block_period, expected_validators_vote_counts_changes);

  // Delegate to validators
  expected_validators_vote_counts_changes.clear();
  size_t txs_count = 0;
  for (size_t i = 0; i < validators_count; i++) {
    const auto delegation_value = (i + 1) * node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
    expected_validators_vote_counts_changes[toAddress(node_cfgs[i].getFirstWallet().node_secret)] = i + 1;
    const auto trx = make_delegate_tx(node_cfgs[i], delegation_value, 1, 1000000000);
    nodes[0]->getTransactionManager()->insertTransaction(trx);
    txs_count++;
  }

  auto new_pillar_block_period = wait_for_next_pillar_block(txs_count);
  checkPillarBlockData(new_pillar_block_period, expected_validators_vote_counts_changes);

  // Undelegate from validators
  expected_validators_vote_counts_changes.clear();
  for (size_t i = 0; i < validators_count - 1; i++) {
    const auto undelegation_value = (i + 1) * node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
    expected_validators_vote_counts_changes[toAddress(node_cfgs[i].getFirstWallet().node_secret)] =
        dev::s256(i + 1) * -1;
    const auto trx = make_undelegate_tx(node_cfgs[i], undelegation_value, 2, 1000000000);
    nodes[0]->getTransactionManager()->insertTransaction(trx);
    txs_count++;
  }

  new_pillar_block_period = wait_for_next_pillar_block(txs_count);
  checkPillarBlockData(new_pillar_block_period, expected_validators_vote_counts_changes);

  // Redelegate
  const auto redelegate_to_addr = toAddress(node_cfgs[node_cfgs.size() - 1].getFirstWallet().node_secret);
  expected_validators_vote_counts_changes.clear();
  expected_validators_vote_counts_changes[redelegate_to_addr] = 0;
  for (size_t i = 0; i < validators_count - 3; i++) {
    const auto node_addr = toAddress(node_cfgs[i].getFirstWallet().node_secret);
    const auto node_vote_count = nodes[0]->getFinalChain()->dposEligibleVoteCount(new_pillar_block_period, node_addr);
    const auto redelegation_value = node_vote_count * node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
    expected_validators_vote_counts_changes[node_addr] = dev::s256(node_vote_count) * -1;
    expected_validators_vote_counts_changes[redelegate_to_addr] += dev::s256(node_vote_count);
    const auto trx = make_redelegate_tx(node_cfgs[i], redelegation_value, redelegate_to_addr, 3, 1000);
    nodes[0]->getTransactionManager()->insertTransaction(trx);
    txs_count++;
  }

  new_pillar_block_period = wait_for_next_pillar_block(txs_count);
  checkPillarBlockData(new_pillar_block_period, expected_validators_vote_counts_changes);
}

TEST_F(PillarChainTest, pillar_chain_syncing) {
  auto node_cfgs = make_node_cfgs(2, 1, 10);

  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_blocks_interval = 4;
  }

  // Start first node
  auto node1 = launch_nodes({node_cfgs[0]})[0];

  // Wait until node1 creates at least 3 pillar blocks
  const auto pillar_blocks_count = 3;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node1->getFinalChain()->lastBlockNumber(),
                   pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval)
  });
  node1->getPbftManager()->stop();

  // Start second node
  auto node2 = launch_nodes({node_cfgs[1]})[0];
  // Wait until node2 syncs pbft chain with node1
  ASSERT_HAPPENS({20s, 200ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getFinalChain()->lastBlockNumber(), node1->getFinalChain()->lastBlockNumber())
  });
  node2->getPbftManager()->stop();

  // Node 2 should not have yet finalized pillar block with period pillar_blocks_count * pillar_blocks_interval
  const auto node2_latest_finalized_pillar_block = node2->getDB()->getLatestPillarBlock();
  ASSERT_TRUE(node2_latest_finalized_pillar_block);
  ASSERT_EQ(node2_latest_finalized_pillar_block->getPeriod(),
            (pillar_blocks_count - 1) * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval);

  // Node 2 should have already created pillar block with period pillar_blocks_count * pillar_blocks_interval
  const auto node2_current_pillar_block = node2->getPillarChainManager()->getCurrentPillarBlock();
  ASSERT_TRUE(node2_current_pillar_block != nullptr);
  ASSERT_EQ(node2_current_pillar_block->getPeriod(),
            pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval);

  auto above_threshold_pillar_votes = node2->getPillarChainManager()->getVerifiedPillarVotes(
      node2_current_pillar_block->getPeriod() + 1, node2_current_pillar_block->getHash(), true);
  ASSERT_TRUE(above_threshold_pillar_votes.empty());

  // Trigger pillar votes syncing for the latest unfinalized pillar block
  node2->getNetwork()->requestPillarBlockVotesBundle(node2_current_pillar_block->getPeriod() + 1,
                                                     node2_current_pillar_block->getHash());

  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    above_threshold_pillar_votes = node2->getPillarChainManager()->getVerifiedPillarVotes(
        node2_current_pillar_block->getPeriod() + 1, node2_current_pillar_block->getHash(), true);
    WAIT_EXPECT_TRUE(ctx, !above_threshold_pillar_votes.empty())
  });

  const auto threshold =
      node2->getPillarChainManager()->getPillarConsensusThreshold(node2_current_pillar_block->getPeriod());
  size_t votes_count = 0;
  for (const auto& pillar_vote : above_threshold_pillar_votes) {
    ASSERT_EQ(pillar_vote->getPeriod() - 1, node2_current_pillar_block->getPeriod());
    ASSERT_EQ(pillar_vote->getBlockHash(), node2_current_pillar_block->getHash());
    votes_count +=
        node2->getFinalChain()->dposEligibleVoteCount(pillar_vote->getPeriod() - 1, pillar_vote->getVoterAddr());
  }
  ASSERT_GE(votes_count, threshold);
}

TEST_F(PillarChainTest, block_serialization) {
  {
    std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange> validator_votes_count_changes;
    validator_votes_count_changes.emplace_back(pillar_chain::PillarBlock::ValidatorVoteCountChange(addr_t(1), -1));
    validator_votes_count_changes.emplace_back(pillar_chain::PillarBlock::ValidatorVoteCountChange(addr_t(2), 2));
    validator_votes_count_changes.emplace_back(pillar_chain::PillarBlock::ValidatorVoteCountChange(addr_t(3), -3));
    validator_votes_count_changes.emplace_back(pillar_chain::PillarBlock::ValidatorVoteCountChange(addr_t(4), 4));
    validator_votes_count_changes.emplace_back(pillar_chain::PillarBlock::ValidatorVoteCountChange(addr_t(5), -5));
    auto pb =
        pillar_chain::PillarBlock(11, h256(22), h256(33), blk_hash_t(44), 55, std::move(validator_votes_count_changes));
    const auto bt = pb.encodeSolidity();
    // This hardcoded hex string is from bridge solidity test(TaraClient.t.sol/test_blockEncodeDecode)
    const auto expected = dev::jsToBytes(
        "00000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000"
        "00000000000000000b00000000000000000000000000000000000000000000000000000000000000160000000000000000000000000000"
        "000000000000000000000000000000000021000000000000000000000000000000000000000000000000000000000000002c0000000000"
        "00000000000000000000000000000000000000000000000000003700000000000000000000000000000000000000000000000000000000"
        "000000c0000000000000000000000000000000000000000000000000000000000000000500000000000000000000000000000000000000"
        "00000000000000000000000001ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff00000000000000000000"
        "00000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000200"
        "00000000000000000000000000000000000000000000000000000000000003ffffffffffffffffffffffffffffffffffffffffffffffff"
        "fffffffffffffffd0000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000"
        "00000000000000000000000000000000040000000000000000000000000000000000000000000000000000000000000005ffffffffffff"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffb");
    ASSERT_EQ(bt, expected);
  }
  {
    std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange> validator_votes_count_changes;
    validator_votes_count_changes.emplace_back(addr_t(1), -1);
    validator_votes_count_changes.emplace_back(addr_t(2), 2);
    validator_votes_count_changes.emplace_back(addr_t(3), -3);
    validator_votes_count_changes.emplace_back(addr_t(4), 4);
    validator_votes_count_changes.emplace_back(addr_t(5), -5);
    validator_votes_count_changes.emplace_back(addr_t("0x290DEcD9548b62A8D60345A988386Fc84Ba6BC95"), 1215134324);
    validator_votes_count_changes.emplace_back(addr_t("0xB10e2D527612073B26EeCDFD717e6a320cF44B4A"), -112321);
    validator_votes_count_changes.emplace_back(addr_t("0x405787FA12A823e0F2b7631cc41B3bA8828b3321"), -1353468546);
    validator_votes_count_changes.emplace_back(addr_t("0xc2575a0E9E593c00f959F8C92f12dB2869C3395a"), 997698769);
    validator_votes_count_changes.emplace_back(addr_t("0x8a35AcfbC15Ff81A39Ae7d344fD709f28e8600B4"), 465876798);

    auto pb =
        pillar_chain::PillarBlock(11, h256(22), h256(33), blk_hash_t(44), 55, std::move(validator_votes_count_changes));
    const auto bt = pb.encodeSolidity();
    // This hardcoded hex string is from bridge solidity test(TaraClient.t.sol/test_blockEncodeDecode)
    const auto expected = dev::jsToBytes(
        "00000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000"
        "00000000000000000b00000000000000000000000000000000000000000000000000000000000000160000000000000000000000000000"
        "000000000000000000000000000000000021000000000000000000000000000000000000000000000000000000000000002c0000000000"
        "00000000000000000000000000000000000000000000000000003700000000000000000000000000000000000000000000000000000000"
        "000000c0000000000000000000000000000000000000000000000000000000000000000a00000000000000000000000000000000000000"
        "00000000000000000000000001ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff00000000000000000000"
        "00000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000200"
        "00000000000000000000000000000000000000000000000000000000000003ffffffffffffffffffffffffffffffffffffffffffffffff"
        "fffffffffffffffd0000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000"
        "00000000000000000000000000000000040000000000000000000000000000000000000000000000000000000000000005ffffffffffff"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffb000000000000000000000000290decd9548b62a8d60345a988386fc84b"
        "a6bc9500000000000000000000000000000000000000000000000000000000486d7a74000000000000000000000000b10e2d527612073b"
        "26eecdfd717e6a320cf44b4afffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe493f0000000000000000000000"
        "00405787fa12a823e0f2b7631cc41b3ba8828b3321ffffffffffffffffffffffffffffffffffffffffffffffffffffffffaf53b57e0000"
        "00000000000000000000c2575a0e9e593c00f959f8c92f12db2869c3395a00000000000000000000000000000000000000000000000000"
        "0000003b77acd10000000000000000000000008a35acfbc15ff81a39ae7d344fd709f28e8600b400000000000000000000000000000000"
        "0000000000000000000000001bc4b73e");
    ASSERT_EQ(bt, expected);
  }
}

TEST_F(PillarChainTest, compact_signature) {
  // This test cases are taken from https://eips.ethereum.org/EIPS/eip-2098

  // Private Key: 0x1234567890123456789012345678901234567890123456789012345678901234
  // Message: "Hello World"
  // Signature:
  //   r:  0x68a020a209d3d56c46f38cc50a33f704f4a9a10a59377f8dd762ac66910e9b90
  //   s:  0x7e865ad05c4035ab5792787d4a0297a43617ae897930a6fe4d822b8faea52064
  //   v:  27
  // Compact Signature:
  //   r:           0x68a020a209d3d56c46f38cc50a33f704f4a9a10a59377f8dd762ac66910e9b90
  //   yParityAndS: 0x7e865ad05c4035ab5792787d4a0297a43617ae897930a6fe4d822b8faea52064
  {
    auto ss = dev::SignatureStruct();
    ss.r = dev::h256("0x68a020a209d3d56c46f38cc50a33f704f4a9a10a59377f8dd762ac66910e9b90");
    ss.s = dev::h256("0x7e865ad05c4035ab5792787d4a0297a43617ae897930a6fe4d822b8faea52064");
    ss.v = 0;
    const auto compact_sig = dev::toCompact(ss);
    // compact sig = r + yParityAndS
    ASSERT_EQ(compact_sig, dev::h512("0x68a020a209d3d56c46f38cc50a33f704f4a9a10a59377f8dd762ac66910e9b907e865ad05c4035a"
                                     "b5792787d4a0297a43617ae897930a6fe4d822b8faea52064"));
  }

  // Private Key: 0x1234567890123456789012345678901234567890123456789012345678901234
  // Message: "It's a small(er) world"
  // Signature:
  //   r:  0x9328da16089fcba9bececa81663203989f2df5fe1faa6291a45381c81bd17f76
  //   s:  0x139c6d6b623b42da56557e5e734a43dc83345ddfadec52cbe24d0cc64f550793
  //   v:  28
  // Compact Signature:
  //   r:           0x9328da16089fcba9bececa81663203989f2df5fe1faa6291a45381c81bd17f76
  //   yParityAndS: 0x939c6d6b623b42da56557e5e734a43dc83345ddfadec52cbe24d0cc64f550793
  {
    auto ss = dev::SignatureStruct();
    ss.r = dev::h256("0x9328da16089fcba9bececa81663203989f2df5fe1faa6291a45381c81bd17f76");
    ss.s = dev::h256("0x139c6d6b623b42da56557e5e734a43dc83345ddfadec52cbe24d0cc64f550793");
    ss.v = 1;
    const auto compact_sig = dev::toCompact(ss);
    // compact sig = r + yParityAndS
    ASSERT_EQ(compact_sig, dev::h512("0x9328da16089fcba9bececa81663203989f2df5fe1faa6291a45381c81bd17f76939c6d6b623b42d"
                                     "a56557e5e734a43dc83345ddfadec52cbe24d0cc64f550793"));
  }
}

TEST_F(PillarChainTest, pillar_block_solidity_rlp_encoding) {
  EthBlockNumber pillar_block_period(123);
  h256 state_root(456);
  h256 bridge_root(789);
  u256 epoch = 0;
  blk_hash_t previous_pillar_block_hash(789);

  std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange> votes_count_changes;
  const auto pillar_block = pillar_chain::PillarBlock(pillar_block_period, state_root, previous_pillar_block_hash,
                                                      bridge_root, epoch, std::move(votes_count_changes));

  auto validateDecodedPillarBlock = [&](const pillar_chain::PillarBlock& pillar_block) {
    ASSERT_EQ(pillar_block.getPeriod(), pillar_block_period);
    ASSERT_EQ(pillar_block.getStateRoot(), state_root);
    ASSERT_EQ(pillar_block.getBridgeRoot(), bridge_root);
    ASSERT_EQ(pillar_block.getPreviousBlockHash(), previous_pillar_block_hash);
    const auto decoded_votes_count_changes = pillar_block.getValidatorsVoteCountsChanges();
    EXPECT_EQ(decoded_votes_count_changes.size(), votes_count_changes.size());
    for (size_t idx = 0; idx < decoded_votes_count_changes.size(); idx++) {
      EXPECT_EQ(decoded_votes_count_changes[idx].addr_, votes_count_changes[idx].addr_);
      EXPECT_EQ(decoded_votes_count_changes[idx].vote_count_change_, votes_count_changes[idx].vote_count_change_);
    }
  };

  auto pillar_block_solidity_encoded = pillar_block.encodeSolidity();
  // TODO[2733]: what hardcoded value to use ???
  // ASSERT_EQ(pillar_block_solidity_encoded.size(), util::EncodingSolidity::kWordSize * 4);
  // TODO[2733]: implement decodeSolidity
  auto decodedBlock = pillar_chain::PillarBlock::decodeSolidity(pillar_block_solidity_encoded);
  validateDecodedPillarBlock(decodedBlock);

  auto pillar_block_rlp_encoded = pillar_block.getRlp();
  validateDecodedPillarBlock(pillar_chain::PillarBlock(dev::RLP(pillar_block_rlp_encoded)));
}

TEST_F(PillarChainTest, pillar_vote_solidity_rlp_encoding) {
  auto pk = dev::Secret(dev::sha3(dev::jsToBytes("0x1")));
  PbftPeriod period{12};
  blk_hash_t block_hash{34};
  auto pillar_vote = PillarVote(pk, period, block_hash);

  auto pillar_vote_solidity_encoded = pillar_vote.encodeSolidity(true);
  ASSERT_EQ(pillar_vote_solidity_encoded.size(), util::EncodingSolidity::kWordSize * 4);

  auto validateDecodedPillarVote = [&](const PillarVote& pillar_vote) {
    ASSERT_EQ(pillar_vote.getPeriod(), period);
    ASSERT_EQ(pillar_vote.getBlockHash(), block_hash);
    ASSERT_EQ(pillar_vote.getVoteSignature(), pillar_vote.getVoteSignature());
  };

  validateDecodedPillarVote(PillarVote::decodeSolidity(pillar_vote_solidity_encoded));
  validateDecodedPillarVote(PillarVote(pillar_vote.rlp()));
}

// contract BridgeMock {
//   address public lightClient;

//   address[] public tokenAddresses;
//   mapping(address => address) public connectors;
//   mapping(address => address) public localAddress;
//   uint256 finalizedEpoch;
//   uint256 appliedEpoch;
//   bytes32 bridgeRoot;
//   event Finalized(uint256 epoch, bytes32 bridgeRoot);
//   function finalizeEpoch() public {
//     bridgeRoot = bytes32(appliedEpoch++);
//     emit Finalized(appliedEpoch - 1, bridgeRoot);
//   }

//   function getBridgeRoot() public view returns(bytes32) { return bridgeRoot; }
//   function shouldFinalizeEpoch() public view returns(bool) { return true; }
// }

auto bridge_mock_bytecode =
    "6080604052348015600e575f80fd5b506105318061001c5f395ff3fe608060405234801561000f575f80fd5b506004361061007b575f3560e0"
    "1c806376081bd51161005957806376081bd5146100eb57806382ae9ef71461011b578063b5700e6814610125578063e5df8b84146101435761"
    "007b565b80630e53aae91461007f578063567041cd146100af578063695a253f146100cd575b5f80fd5b610099600480360381019061009491"
    "90610309565b610173565b6040516100a69190610343565b60405180910390f35b6100b76101a3565b6040516100c49190610376565b604051"
    "80910390f35b6100d56101ab565b6040516100e291906103a7565b60405180910390f35b61010560048036038101906101009190610309565b"
    "6101b4565b6040516101129190610343565b60405180910390f35b6101236101e4565b005b61012d61024d565b60405161013a919061034356"
    "5b60405180910390f35b61015d600480360381019061015891906103f3565b610270565b60405161016a9190610343565b60405180910390f3"
    "5b6002602052805f5260405f205f915054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b5f6001905090565b"
    "5f600654905090565b6003602052805f5260405f205f915054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b"
    "60055f8154809291906101f69061044b565b919050555f1b6006819055507fa05a0e9561eff1f01a29e7a680d5957bb7312e5766a8da1f494b"
    "6d6ac18031f460016005546102329190610492565b6006546040516102439291906104d4565b60405180910390a1565b5f8054906101000a90"
    "0473ffffffffffffffffffffffffffffffffffffffff1681565b6001818154811061027f575f80fd5b905f5260205f20015f91505490610100"
    "0a900473ffffffffffffffffffffffffffffffffffffffff1681565b5f80fd5b5f73ffffffffffffffffffffffffffffffffffffffff821690"
    "50919050565b5f6102d8826102af565b9050919050565b6102e8816102ce565b81146102f2575f80fd5b50565b5f81359050610303816102df"
    "565b92915050565b5f6020828403121561031e5761031d6102ab565b5b5f61032b848285016102f5565b91505092915050565b61033d816102"
    "ce565b82525050565b5f6020820190506103565f830184610334565b92915050565b5f8115159050919050565b6103708161035c565b825250"
    "50565b5f6020820190506103895f830184610367565b92915050565b5f819050919050565b6103a18161038f565b82525050565b5f60208201"
    "90506103ba5f830184610398565b92915050565b5f819050919050565b6103d2816103c0565b81146103dc575f80fd5b50565b5f8135905061"
    "03ed816103c9565b92915050565b5f60208284031215610408576104076102ab565b5b5f610415848285016103df565b91505092915050565b"
    "7f4e487b71000000000000000000000000000000000000000000000000000000005f52601160045260245ffd5b5f610455826103c0565b9150"
    "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82036104875761048661041e565b5b600182019050919050"
    "565b5f61049c826103c0565b91506104a7836103c0565b92508282039050818111156104bf576104be61041e565b5b92915050565b6104ce81"
    "6103c0565b82525050565b5f6040820190506104e75f8301856104c5565b6104f46020830184610398565b939250505056fea2646970667358"
    "22122085e3f65e550b0efa67168cceadab882db443d5c0057e7f286914bc35c257196064736f6c634300081a0033";

TEST_F(PillarChainTest, finalize_root_in_pillar_block) {
  auto node_cfgs = make_node_cfgs(2, 2, 10);
  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_blocks_interval = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.bridge_contract_address =
        dev::Address("0xc5b7d26bec6acdc3a0d33fe4c70be346a47a3a33");
  }

  auto nodes = launch_nodes(node_cfgs);
  auto node = nodes[0];

  uint64_t nonce = 0, trxs_count = node->getDB()->getNumTransactionExecuted();
  auto deploy_bridge_mock =
      std::make_shared<Transaction>(nonce++, 0, 1000000000, TEST_TX_GAS_LIMIT, dev::fromHex(bridge_mock_bytecode),
                                    node->getConfig().getFirstWallet().node_secret);
  node->getTransactionManager()->insertTransaction(deploy_bridge_mock);
  trxs_count++;

  // wait for trx execution
  EXPECT_HAPPENS({20s, 1s}, [&](auto& ctx) {
    if (ctx.fail_if(node->getDB()->getNumTransactionExecuted() != trxs_count)) {
      return;
    }
  });

  // Wait until nodes create at least 4 pillar blocks
  const auto pillar_blocks_count = 4;
  const auto min_amount_of_pbft_blocks =
      pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval;
  ASSERT_HAPPENS({30s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(), min_amount_of_pbft_blocks + 1)
    }
  });

  for (auto& node : nodes) {
    // Check if right amount of pillar blocks were created
    const auto latest_pillar_block = node->getDB()->getLatestPillarBlock();
    ASSERT_TRUE(latest_pillar_block);
    ASSERT_EQ(latest_pillar_block->getPeriod(),
              pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval);
  }

  for (auto& node : nodes) {
    for (auto epoch = 0; epoch < 4; epoch++) {
      auto period = (epoch + 1) * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_blocks_interval;
      const auto pillar_block = node->getDB()->getPillarBlock(period);
      ASSERT_TRUE(pillar_block);
      ASSERT_EQ(pillar_block->getPeriod(), period);
      ASSERT_EQ(u256(pillar_block->getBridgeRoot()), u256(epoch));
      // check system transactions
      {
        // check that we are getting this transaction from the final chain
        auto trxs = node->getFinalChain()->transactions(period - 1);
        ASSERT_EQ(trxs.size(), 1);
        // check that this 1 transaction is system transaction
        const auto& trx = trxs.at(0);
        ASSERT_EQ(trx->getSender(), kTaraxaSystemAccount);
        ASSERT_EQ(trx->getReceiver(), node_cfgs[0].genesis.state.hardforks.ficus_hf.bridge_contract_address);
        // check that correct hash is returned
        auto hashes = node->getFinalChain()->transactionHashes(period - 1);
        ASSERT_EQ(hashes->size(), 1);
        ASSERT_EQ(hashes->at(0), trx->getHash());
        // check that location by hash exists and is_system set to true
        const auto& trx_loc = node->getDB()->getTransactionLocation(trx->getHash());
        EXPECT_TRUE(trx_loc.has_value());
        ASSERT_EQ(trx_loc->period, period - 1);
        ASSERT_EQ(trx_loc->position, 0);
        ASSERT_EQ(trx_loc->is_system, true);
        // check that we can get this transaction by hash
        const auto& trx_by_hash = node->getDB()->getTransaction(trx->getHash());
        ASSERT_TRUE(trx_by_hash != nullptr);
        ASSERT_EQ(trx_by_hash->getSender(), kTaraxaSystemAccount);
        ASSERT_EQ(trx_by_hash->getReceiver(), node_cfgs[0].genesis.state.hardforks.ficus_hf.bridge_contract_address);
        ASSERT_EQ(trx_by_hash->getSender(), kTaraxaSystemAccount);
        // check that receipt exists
        const auto& trx_receipt = node->getFinalChain()->transactionReceipt(trx_loc->period, trx_loc->position);
        ASSERT_TRUE(trx_receipt.has_value());
        ASSERT_EQ(trx_receipt->status_code, 1);
      }
    }
  }
}

}  // namespace taraxa::core_tests

TARAXA_TEST_MAIN({})