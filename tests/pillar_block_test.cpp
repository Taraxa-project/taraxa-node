#include "pillar_chain/pillar_block.hpp"

#include <gtest/gtest.h>

#include <iostream>

#include "final_chain/contract_interface.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"
namespace taraxa::core_tests {

struct PillarBlockTest : NodesTest {};

TEST_F(PillarBlockTest, hash) {
  std::vector<pillar_chain::PillarBlock::ValidatorStakeChange> validator_stakes_changes;
  validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(1), dev::s96(1)));
  validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(2), dev::s96(2)));
  validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(3), dev::s96(3)));
  validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(4), dev::s96(4)));
  validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(5), dev::s96(5)));

  pillar_chain::PillarBlock pb1 =
      pillar_chain::PillarBlock(11, h256(22), h256(33), blk_hash_t(44), std::move(validator_stakes_changes));
  auto hash1 = pb1.getHash();
  std::cout << "hash: " << pb1.getHash() << " rlp: " << dev::toHex(pb1.getRlp()) << std::endl;

  const auto bt = pb1.encode();
  std::cout << "pack result: " << dev::toHex(bt) << std::endl;
}

}  // namespace taraxa::core_tests