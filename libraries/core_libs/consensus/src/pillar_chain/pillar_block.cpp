#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"
#include "pillar_chain/bls_signature.hpp"

namespace taraxa::pillar_chain {

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(const state_api::ValidatorStake& stake)
    : addr_(stake.addr), stake_change_(dev::s256(stake.stake)) {}

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(addr_t addr, dev::s256 stake_change)
    : addr_(addr), stake_change_(stake_change) {}

RLP_FIELDS_DEFINE(PillarBlock::ValidatorStakeChange, addr_, stake_change_)

PillarBlock::ValidatorStakeChange::ValidatorStakeChange(const dev::RLP& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), addr_, stake_change_);
}

PillarBlock::PillarBlock(const dev::RLP& rlp) : PillarBlock(util::rlp_dec<PillarBlock>(rlp)) {}

PillarBlock::PillarBlock(PbftPeriod period, h256 state_root,
                         std::vector<ValidatorStakeChange>&& validator_stakes_changes,
                         PillarBlock::Hash previous_pillar_block_hash)
    : period_(period),
      state_root_(state_root),
      validators_stakes_changes_(std::move(validator_stakes_changes)),
      previous_pillar_block_hash_(previous_pillar_block_hash) {}

PillarBlock::PillarBlock(const PillarBlock& pillar_block)
    : period_(pillar_block.period_),
      state_root_(pillar_block.state_root_),
      validators_stakes_changes_(pillar_block.validators_stakes_changes_),
      previous_pillar_block_hash_(pillar_block.previous_pillar_block_hash_) {}

dev::bytes PillarBlock::getRlp() const { return util::rlp_enc(*this); }

PbftPeriod PillarBlock::getPeriod() const { return period_; }

PillarBlock::Hash PillarBlock::getPreviousBlockHash() const { return previous_pillar_block_hash_; }

PillarBlock::Hash PillarBlock::getHash() {
  {
    std::shared_lock lock(hash_mutex_);
    if (hash_.has_value()) {
      return *hash_;
    }
  }

  std::scoped_lock lock(hash_mutex_);
  hash_ = dev::sha3(getRlp());
  return *hash_;
}

RLP_FIELDS_DEFINE(PillarBlock, period_, state_root_, previous_pillar_block_hash_, validators_stakes_changes_)
RLP_FIELDS_DEFINE(PillarBlockData, block, signatures)

}  // namespace taraxa::pillar_chain
