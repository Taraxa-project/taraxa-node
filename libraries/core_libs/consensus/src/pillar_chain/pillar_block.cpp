#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

dev::bytes PillarBlock::ValidatorStakeChange::getRlp() const {
  dev::RLPStream s(2);
  s << addr;
  s << stake;

  return s.invalidate();
}

PillarBlock::PillarBlock(PbftPeriod period, h256 state_root,
                         std::vector<ValidatorStakeChange>&& validator_stakes_changes,
                         PillarBlock::Hash previous_pillar_block_hash)
    : period_(period),
      state_root(state_root),
      validators_stakes_changes_(std::move(validator_stakes_changes)),
      previous_pillar_block_hash_(previous_pillar_block_hash),
      kCachedHash(dev::sha3(getRlp())) {}

dev::bytes PillarBlock::getRlp() const {
  dev::RLPStream s(4);
  s << period_;
  s << state_root;
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
