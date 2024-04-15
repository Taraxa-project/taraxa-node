#include <gtest/gtest.h>

#include <iostream>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "pbft/pbft_manager.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

struct PillarChainTest : NodesTest {};

TEST_F(PillarChainTest, pillar_chain_db) {
  auto db_ptr = std::make_shared<DbStorage>(data_dir);
  auto& db = *db_ptr;

  // Pillar chain
  EthBlockNumber pillar_block_num(123);
  h256 state_root(456);
  blk_hash_t previous_pillar_block_hash(789);

  std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange> votes_count_changes;
  const auto vote_count_change1 = votes_count_changes.emplace_back(addr_t(1), 1);
  const auto vote_count_change2 = votes_count_changes.emplace_back(addr_t(2), 2);

  const auto pillar_block = std::make_shared<pillar_chain::PillarBlock>(
      pillar_block_num, state_root, h256{}, std::move(votes_count_changes), previous_pillar_block_hash);

  // Pillar block
  auto batch = db.createWriteBatch();
  db.saveCurrentPillarBlock(pillar_block, batch);
  db.commitWriteBatch(batch);

  const auto pillar_block_db = db.getCurrentPillarBlock();
  EXPECT_EQ(pillar_block->getHash(), pillar_block_db->getHash());

  // Pillar block data
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;
  const auto vote1 = pillar_votes.emplace_back(
      std::make_shared<PillarVote>(secret_t::random(), pillar_block->getPeriod(), pillar_block->getHash()));
  const auto vote2 = pillar_votes.emplace_back(
      std::make_shared<PillarVote>(secret_t::random(), pillar_block->getPeriod(), pillar_block->getHash()));

  const auto previous_pillar_block = std::make_shared<pillar_chain::PillarBlock>(
      pillar_block_num - 1, h256{}, h256{}, std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange>{},
      blk_hash_t{});
  db.savePillarBlockData(
      pillar_chain::PillarBlockData{pillar_block, std::vector<std::shared_ptr<PillarVote>>{pillar_votes}});
  db.savePillarBlockData(
      pillar_chain::PillarBlockData{previous_pillar_block, std::vector<std::shared_ptr<PillarVote>>{pillar_votes}});

  const auto pillar_block_data_db = db.getPillarBlockData(pillar_block->getPeriod());
  EXPECT_EQ(pillar_block->getHash(), pillar_block_data_db->block_->getHash());
  EXPECT_EQ(pillar_votes.size(), pillar_block_data_db->pillar_votes_.size());
  for (size_t idx = 0; idx < pillar_votes.size(); idx++) {
    EXPECT_EQ(pillar_votes[idx]->getHash(), pillar_block_data_db->pillar_votes_[idx]->getHash());
  }

  const auto latest_pillar_block_data_db = db.getLatestPillarBlockData();
  EXPECT_EQ(pillar_block->getHash(), latest_pillar_block_data_db->block_->getHash());
  EXPECT_EQ(pillar_votes.size(), latest_pillar_block_data_db->pillar_votes_.size());
  for (size_t idx = 0; idx < pillar_votes.size(); idx++) {
    EXPECT_EQ(pillar_votes[idx]->getHash(), latest_pillar_block_data_db->pillar_votes_[idx]->getHash());
  }

  // Pillar block stakes
  std::vector<state_api::ValidatorVoteCount> vote_counts;
  const auto stake1 = votes_count_changes.emplace_back(addr_t(123), 123);
  const auto stake2 = votes_count_changes.emplace_back(addr_t(456), 456);

  batch = db.createWriteBatch();
  db.saveCurrentPillarBlockVoteCounts(vote_counts, batch);
  db.commitWriteBatch(batch);

  const auto current_pillar_block_vote_counts_db = db.getCurrentPillarBlockVoteCounts();
  EXPECT_EQ(vote_counts.size(), current_pillar_block_vote_counts_db.size());
  for (size_t idx = 0; idx < current_pillar_block_vote_counts_db.size(); idx++) {
    EXPECT_EQ(current_pillar_block_vote_counts_db[idx].addr, current_pillar_block_vote_counts_db[idx].addr);
    EXPECT_EQ(current_pillar_block_vote_counts_db[idx].vote_count, current_pillar_block_vote_counts_db[idx].vote_count);
  }

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
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 3;
    node_cfg.genesis.state.hardforks.ficus_hf.pbft_inclusion_delay = 2;
  }

  auto nodes = launch_nodes(node_cfgs);

  // Wait until nodes create at least 2 pillar blocks
  const auto pillar_blocks_count = 2;
  const auto min_amount_of_pbft_blocks =
      pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(),
                     min_amount_of_pbft_blocks + node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
    }
  });

  for (auto& node : nodes) {
    node->getPbftManager()->stop();

    // Check if right amount of pillar blocks were created
    const auto latest_pillar_block_data = node->getDB()->getLatestPillarBlockData();
    ASSERT_TRUE(latest_pillar_block_data.has_value());
    ASSERT_EQ(latest_pillar_block_data->block_->getPeriod(),
              pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods);
  }
}

TEST_F(PillarChainTest, votes_count_changes) {
  const auto validators_count = 3;
  auto node_cfgs = make_node_cfgs(validators_count, validators_count, 10);

  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 3;
    node_cfg.genesis.state.hardforks.ficus_hf.pbft_inclusion_delay = 2;
  }

  std::vector<dev::s256> validators_vote_counts;
  validators_vote_counts.reserve(node_cfgs.size());
  for (const auto& validator : node_cfgs[0].genesis.state.dpos.initial_validators) {
    auto& vote_count = validators_vote_counts.emplace_back(0);
    for (const auto& delegation : validator.delegations) {
      vote_count += delegation.second / node_cfgs[0].genesis.state.dpos.vote_eligibility_balance_step;
    }
  }

  auto nodes = launch_nodes(node_cfgs);

  // Wait until nodes create first pillar block
  const auto first_pillar_block_period = node_cfgs[0].genesis.state.hardforks.ficus_hf.firstPillarBlockPeriod();
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(),
                     first_pillar_block_period + node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
    }
  });

  // Check if vote_counts changes in first pillar block == initial validators vote_counts
  for (auto& node : nodes) {
    // Check if right amount of pillar blocks were created
    const auto first_pillar_block_data = node->getDB()->getPillarBlockData(first_pillar_block_period);
    ASSERT_TRUE(first_pillar_block_data.has_value());

    ASSERT_EQ(first_pillar_block_data->block_->getPeriod(), first_pillar_block_period);
    ASSERT_EQ(first_pillar_block_data->block_->getValidatorsVoteCountsChanges().size(), validators_count);
    size_t idx = 0;
    for (const auto& vote_count_change : first_pillar_block_data->block_->getValidatorsVoteCountsChanges()) {
      ASSERT_EQ(vote_count_change.vote_count_change_, validators_vote_counts[idx]);
      idx++;
    }
  }

  // Change validators delegation
  const auto delegation_value = 2 * node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
  const auto vote_count_change_validators_count = validators_count - 1;
  for (size_t i = 0; i < vote_count_change_validators_count; i++) {
    const auto trx = make_delegate_tx(node_cfgs[i], delegation_value, 1, 1000);
    nodes[0]->getTransactionManager()->insertTransaction(trx);
  }

  PbftPeriod executed_delegations_pbft_period = 1000000;
  EXPECT_HAPPENS({20s, 1s}, [&](auto& ctx) {
    for (auto& node : nodes) {
      if (ctx.fail_if(node->getDB()->getNumTransactionExecuted() != vote_count_change_validators_count)) {
        return;
      }
      const auto chain_size = node->getPbftChain()->getPbftChainSize();
      if (chain_size < executed_delegations_pbft_period) {
        executed_delegations_pbft_period = chain_size;
      }
    }
  });

  // Wait until new pillar block with changed validators vote_counts is created
  const auto new_pillar_block_period =
      executed_delegations_pbft_period -
      executed_delegations_pbft_period % node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods +
      node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(),
                     new_pillar_block_period + node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
    }
  });

  // Check if vote_counts changes in new pillar block changed according to new delegations
  for (auto& node : nodes) {
    // Check if right amount of pillar blocks were created
    const auto new_pillar_block_data = node->getDB()->getPillarBlockData(new_pillar_block_period);
    ASSERT_TRUE(new_pillar_block_data.has_value());
    ASSERT_EQ(new_pillar_block_data->block_->getPeriod(), new_pillar_block_period);
    ASSERT_EQ(new_pillar_block_data->block_->getValidatorsVoteCountsChanges().size(),
              vote_count_change_validators_count);
    size_t idx = 0;
    for (const auto& vote_count_change : new_pillar_block_data->block_->getValidatorsVoteCountsChanges()) {
      ASSERT_EQ(vote_count_change.vote_count_change_,
                delegation_value / node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold);
      idx++;
    }
  }
}

TEST_F(PillarChainTest, pillar_chain_syncing) {
  auto node_cfgs = make_node_cfgs(2, 1, 10);

  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 3;
    node_cfg.genesis.state.hardforks.ficus_hf.pbft_inclusion_delay = 2;
  }

  // Start first node
  auto node1 = launch_nodes({node_cfgs[0]})[0];

  // Wait until node1 creates at least 3 pillar blocks
  const auto pillar_blocks_count = 3;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_GE(ctx, node1->getPbftChain()->getPbftChainSize(),
                   pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods +
                       node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
  });
  node1->getPbftManager()->stop();

  // Start second node
  auto node2 = launch_nodes({node_cfgs[1]})[0];
  // Wait until node2 syncs pbft chain with node1
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getPbftChain()->getPbftChainSize(), node1->getPbftChain()->getPbftChainSize())
  });

  // Pbft/pillar chain syncing works in a way that pbft block with period N contains pillar votes for pillar block with
  // period N-ficus_hf.pillar_block_periods.
  const auto node2_latest_finalized_pillar_block_data = node2->getDB()->getLatestPillarBlockData();
  ASSERT_TRUE(node2_latest_finalized_pillar_block_data.has_value());
  ASSERT_EQ(node2_latest_finalized_pillar_block_data->block_->getPeriod(),
            (pillar_blocks_count - 1) * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods);

  // Trigger pillar votes syncing
  node2->getPillarChainManager()->checkPillarChainSynced(
      pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods);
  // Wait until node2 gets pillar votes and finalized pillar block #3
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getDB()->getLatestPillarBlockData()->block_->getPeriod(),
                   pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods)
  });
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
        pillar_chain::PillarBlock(11, h256(22), h256(33), std::move(validator_votes_count_changes), blk_hash_t(44));
    const auto bt = pb.encodeSolidity();
    // This hardcoded hex string is from bridge solidity test(TaraClient.t.sol/test_blockEncodeDecode)
    const auto expected = dev::jsToBytes(
        "00000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000"
        "00000000000000000b00000000000000000000000000000000000000000000000000000000000000160000000000000000000000000000"
        "000000000000000000000000000000000021000000000000000000000000000000000000000000000000000000000000002c0000000000"
        "0000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000"
        "000000050000000000000000000000000000000000000000000000000000000000000001ffffffffffffffffffffffffffffffffffffff"
        "ffffffffffffffffffffffffff000000000000000000000000000000000000000000000000000000000000000200000000000000000000"
        "000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000003ff"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffd000000000000000000000000000000000000000000000000"
        "00000000000000040000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000"
        "0000000000000000000000000000000005fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffb");
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
        pillar_chain::PillarBlock(11, h256(22), h256(33), std::move(validator_votes_count_changes), blk_hash_t(44));
    const auto bt = pb.encodeSolidity();
    // This hardcoded hex string is from bridge solidity test(TaraClient.t.sol/test_blockEncodeDecode)
    const auto expected = dev::jsToBytes(
        "0x000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000"
        "0000000000000000000b000000000000000000000000000000000000000000000000000000000000001600000000000000000000000000"
        "00000000000000000000000000000000000021000000000000000000000000000000000000000000000000000000000000002c00000000"
        "000000000000000000000000000000000000000000000000000000a0000000000000000000000000000000000000000000000000000000"
        "000000000a0000000000000000000000000000000000000000000000000000000000000001ffffffffffffffffffffffffffffffffffff"
        "ffffffffffffffffffffffffffff0000000000000000000000000000000000000000000000000000000000000002000000000000000000"
        "00000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000003"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffd0000000000000000000000000000000000000000000000"
        "00000000000000000400000000000000000000000000000000000000000000000000000000000000040000000000000000000000000000"
        "000000000000000000000000000000000005fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffb0000000000"
        "00000000000000290decd9548b62a8d60345a988386fc84ba6bc9500000000000000000000000000000000000000000000000000000000"
        "486d7a74000000000000000000000000b10e2d527612073b26eecdfd717e6a320cf44b4affffffffffffffffffffffffffffffffffffff"
        "fffffffffffffffffffffe493f000000000000000000000000405787fa12a823e0f2b7631cc41b3ba8828b3321ffffffffffffffffffff"
        "ffffffffffffffffffffffffffffffffffffaf53b57e000000000000000000000000c2575a0e9e593c00f959f8c92f12db2869c3395a00"
        "0000000000000000000000000000000000000000000000000000003b77acd10000000000000000000000008a35acfbc15ff81a39ae7d34"
        "4fd709f28e8600b4000000000000000000000000000000000000000000000000000000001bc4b73e");
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
  EthBlockNumber pillar_block_num(123);
  h256 state_root(456);
  h256 bridge_root(789);
  blk_hash_t previous_pillar_block_hash(789);

  std::vector<pillar_chain::PillarBlock::ValidatorVoteCountChange> votes_count_changes;
  const auto vote_count_change1 = votes_count_changes.emplace_back(addr_t(1), 1);
  const auto vote_count_change2 = votes_count_changes.emplace_back(addr_t(2), 2);

  auto vcc = votes_count_changes;
  const auto pillar_block =
      pillar_chain::PillarBlock(pillar_block_num, state_root, bridge_root, std::move(vcc), previous_pillar_block_hash);

  auto validateDecodedPillarBlock = [&](const pillar_chain::PillarBlock& pillar_block) {
    ASSERT_EQ(pillar_block.getPeriod(), pillar_block_num);
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

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}