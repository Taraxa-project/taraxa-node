#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(const state_api::ValidatorStake& stake)
    : addr_(stake.addr), stake_change_(dev::s256(stake.stake)) {}

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(addr_t addr, dev::s256 stake_change)
    : addr_(addr), stake_change_(stake_change) {}

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(const dev::RLP& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), addr_, stake_change_);
}

dev::bytes PillarBlock::ValidatorStakeChange::getRlp() const {
  dev::RLPStream s(2);
  s << addr_;
  s << stake_change_;

  return s.invalidate();
}

PillarBlock::PillarBlock(const dev::RLP& rlp) {
  dev::bytes stakes_changes;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), period_, state_root_, previous_pillar_block_hash_, stakes_changes);

  dev::RLP stakes_changes_rlp(stakes_changes);

  validators_stakes_changes_.reserve(stakes_changes_rlp.itemCount());
  for (const auto stake_change_rlp : stakes_changes_rlp) {
    validators_stakes_changes_.emplace_back(stake_change_rlp);
  }
}

PillarBlock::PillarBlock(PbftPeriod period, h256 state_root,
                         std::vector<ValidatorStakeChange>&& validator_stakes_changes,
                         PillarBlock::Hash previous_pillar_block_hash)
    : period_(period),
      state_root_(state_root),
      validators_stakes_changes_(std::move(validator_stakes_changes)),
      previous_pillar_block_hash_(previous_pillar_block_hash),
      kCachedHash(dev::sha3(getRlp())) {}

dev::bytes PillarBlock::getRlp() const {
  dev::RLPStream s(4);
  s << period_;
  s << state_root_;
  s << previous_pillar_block_hash_;

  s.appendList(validators_stakes_changes_.size());
  for (const auto& stake_change : validators_stakes_changes_) {
    s.appendRaw(stake_change.getRlp());
  }

  return s.invalidate();
}

PbftPeriod PillarBlock::getPeriod() const { return period_; }

PillarBlock::Hash PillarBlock::getHash() const { return kCachedHash; }

}  // namespace taraxa
