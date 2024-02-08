#include "pillar_chain/pillar_block.hpp"

#include <gtest/gtest.h>

#include <iostream>

#include "final_chain/contract_interface.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"
namespace taraxa::core_tests {

struct PillarBlockTest : NodesTest {};

TEST_F(PillarBlockTest, hash) {
  {
    std::vector<pillar_chain::PillarBlock::ValidatorStakeChange> validator_stakes_changes;
    validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(1), dev::s256(-1)));
    validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(2), dev::s256(2)));
    validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(3), dev::s256(-3)));
    validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(4), dev::s256(4)));
    validator_stakes_changes.emplace_back(pillar_chain::PillarBlock::ValidatorStakeChange(addr_t(5), dev::s256(-5)));
    auto pb = pillar_chain::PillarBlock(11, h256(22), h256(33), blk_hash_t(44), std::move(validator_stakes_changes));
    const auto bt = pb.encode();
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
    std::vector<pillar_chain::PillarBlock::ValidatorStakeChange> validator_stakes_changes;
    validator_stakes_changes.emplace_back(addr_t(1), dev::s256(-1));
    validator_stakes_changes.emplace_back(addr_t(2), dev::s256(2));
    validator_stakes_changes.emplace_back(addr_t(3), dev::s256(-3));
    validator_stakes_changes.emplace_back(addr_t(4), dev::s256(4));
    validator_stakes_changes.emplace_back(addr_t(5), dev::s256(-5));

    validator_stakes_changes.emplace_back(addr_t("0x290DEcD9548b62A8D60345A988386Fc84Ba6BC95"),
                                          dev::s256(12151343246432));
    validator_stakes_changes.emplace_back(addr_t("0xB10e2D527612073B26EeCDFD717e6a320cF44B4A"), dev::s256(-112321));
    validator_stakes_changes.emplace_back(addr_t("0x405787FA12A823e0F2b7631cc41B3bA8828b3321"),
                                          dev::s256("-13534685468457923145"));
    validator_stakes_changes.emplace_back(addr_t("0xc2575a0E9E593c00f959F8C92f12dB2869C3395a"), dev::s256(9976987696));
    validator_stakes_changes.emplace_back(addr_t("0x8a35AcfbC15Ff81A39Ae7d344fD709f28e8600B4"),
                                          dev::s256(465876798678065667));

    auto pb = pillar_chain::PillarBlock(11, h256(22), h256(33), blk_hash_t(44), std::move(validator_stakes_changes));
    const auto bt = pb.encode();
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
        "00000000000000290decd9548b62a8d60345a988386fc84ba6bc9500000000000000000000000000000000000000000000000000000b0d"
        "347f6c60000000000000000000000000b10e2d527612073b26eecdfd717e6a320cf44b4affffffffffffffffffffffffffffffffffffff"
        "fffffffffffffffffffffe493f000000000000000000000000405787fa12a823e0f2b7631cc41b3ba8828b3321ffffffffffffffffffff"
        "ffffffffffffffffffffffffffff442b2346b9ec8db7000000000000000000000000c2575a0e9e593c00f959f8c92f12db2869c3395a00"
        "00000000000000000000000000000000000000000000000000000252acc0300000000000000000000000008a35acfbc15ff81a39ae7d34"
        "4fd709f28e8600b40000000000000000000000000000000000000000000000000677207ae64d6203");
    ASSERT_EQ(bt, expected);
  }
}

}  // namespace taraxa::core_tests