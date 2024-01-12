#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa::pillar_chain {

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(const state_api::ValidatorStake& stake)
    : addr_(stake.addr), stake_change_(dev::s256(stake.stake)) {}

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(addr_t addr, dev::s256 stake_change)
    : addr_(addr), stake_change_(stake_change) {}

RLP_FIELDS_DEFINE(PillarBlock::ValidatorStakeChange, addr_, stake_change_)

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(const dev::RLP& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), addr_, stake_change_);
}

PillarBlock::PillarBlock(const dev::RLP& rlp) {
  *this = util::rlp_dec<PillarBlock>(rlp);
  // TODO: try if rlp.data().toBytes() produces the same output as getRlp()
  kCachedHash = dev::sha3(getRlp());
}

PillarBlock::PillarBlock(PbftPeriod period, h256 state_root,
                         std::vector<ValidatorStakeChange>&& validator_stakes_changes,
                         PillarBlock::Hash previous_pillar_block_hash)
    : period_(period),
      state_root_(state_root),
      validators_stakes_changes_(std::move(validator_stakes_changes)),
      previous_pillar_block_hash_(previous_pillar_block_hash),
      kCachedHash(dev::sha3(getRlp())) {}

dev::bytes PillarBlock::getRlp() const { return util::rlp_enc(*this); }

PbftPeriod PillarBlock::getPeriod() const { return period_; }

PillarBlock::Hash PillarBlock::getHash() const { return kCachedHash; }

RLP_FIELDS_DEFINE(PillarBlock, period_, state_root_, previous_pillar_block_hash_, validators_stakes_changes_)

}  // namespace taraxa::pillar_chain
