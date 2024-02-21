#include <gtest/gtest.h>

#include <iostream>

#include "pillar_chain/pillar_block.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"
namespace taraxa::core_tests {

struct PillarTest : NodesTest {};

TEST_F(PillarTest, block_serialization) {
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

TEST_F(PillarTest, compact_signature) {
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
    // const auto compact_sig = dev::toCompact(sig);
    const auto compact_sig = dev::toCompact(ss);
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
    // const auto compact_sig = dev::toCompact(sig);
    const auto compact_sig = dev::toCompact(ss);
    ASSERT_EQ(compact_sig, dev::h512("0x9328da16089fcba9bececa81663203989f2df5fe1faa6291a45381c81bd17f76939c6d6b623b42d"
                                     "a56557e5e734a43dc83345ddfadec52cbe24d0cc64f550793"));
  }
}

TEST_F(PillarTest, vote_encode_decode) {
  auto pk = dev::Secret(dev::sha3(dev::jsToBytes("0x1")));
  auto vote = PillarVote(pk, 1, dev::sha3(blk_hash_t(2)));
  ASSERT_EQ(vote.encode().size(), 32 * 4);
  // std::cout << "signer: " << dev::toAddress(pk).hex() << "\n"
  //           << "signed: " << dev::toHex(vote.encode()) << std::endl;
  auto decoded = PillarVote::decode(vote.encode());
  ASSERT_EQ(decoded.getPeriod(), 1);
  ASSERT_EQ(decoded.getBlockHash(), dev::sha3(blk_hash_t(2)));
  ASSERT_EQ(decoded.getVoteSignature(), vote.getVoteSignature());
}

}  // namespace taraxa::core_tests