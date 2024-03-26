#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"
#include "common/encoding_solidity.hpp"
#include "vote/pillar_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

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

PillarBlock::PillarBlock(PbftPeriod period, h256 state_root, h256 bridge_root,
                         std::vector<ValidatorStakeChange>&& validator_stakes_changes,
                         PillarBlock::Hash previous_pillar_block_hash)
    : period_(period),
      state_root_(state_root),
      bridge_root_(bridge_root),
      validators_stakes_changes_(std::move(validator_stakes_changes)),
      previous_pillar_block_hash_(previous_pillar_block_hash) {}

PillarBlock::PillarBlock(const PillarBlock& pillar_block)
    : period_(pillar_block.period_),
      state_root_(pillar_block.state_root_),
      bridge_root_(pillar_block.bridge_root_),
      validators_stakes_changes_(pillar_block.validators_stakes_changes_),
      previous_pillar_block_hash_(pillar_block.previous_pillar_block_hash_) {}

dev::bytes PillarBlock::getRlp() const { return util::rlp_enc(*this); }

PbftPeriod PillarBlock::getPeriod() const { return period_; }

PillarBlock::Hash PillarBlock::getPreviousBlockHash() const { return previous_pillar_block_hash_; }

const std::vector<PillarBlock::ValidatorStakeChange>& PillarBlock::getValidatorsStakesChanges() const {
  return validators_stakes_changes_;
}

const h256& PillarBlock::getStateRoot() const { return state_root_; }

const h256& PillarBlock::getBridgeRoot() const { return bridge_root_; }

PillarBlock::Hash PillarBlock::getHash() const {
  {
    std::shared_lock lock(hash_mutex_);
    if (hash_.has_value()) {
      return *hash_;
    }
  }

  std::scoped_lock lock(hash_mutex_);
  hash_ = dev::sha3(encodeSolidity());
  return *hash_;
}

Json::Value PillarBlock::getJson() const {
  Json::Value res;
  res["period"] = dev::toJS(period_);
  res["state_root"] = dev::toJS(state_root_);
  res["bridge_root"] = dev::toJS(bridge_root_);
  res["previous_pillar_block_hash"] = dev::toJS(previous_pillar_block_hash_);
  res["validators_stakes_changes"] = Json::Value(Json::arrayValue);
  for (auto const& stake_change : validators_stakes_changes_) {
    Json::Value stake_change_json;
    stake_change_json["address"] = dev::toJS(stake_change.addr_);
    stake_change_json["value"] = dev::toJS(stake_change.stake_change_);

    res["validators_stakes_changes"].append(std::move(stake_change_json));
  }
  res["hash"] = dev::toJS(getHash());

  return res;
}

dev::bytes PillarBlock::encodeSolidity() const {
  dev::bytes result;
  result.reserve((1 + 4 + 2 + 2 * validators_stakes_changes_.size()) * util::EncodingSolidity::kWordSize);

  auto start = util::EncodingSolidity::pack(32);
  result.insert(result.end(), start.begin(), start.end());

  auto body = util::EncodingSolidity::pack(period_, state_root_, bridge_root_, previous_pillar_block_hash_);
  result.insert(result.end(), body.begin(), body.end());

  auto array_data = util::EncodingSolidity::pack(160, validators_stakes_changes_.size());
  result.insert(result.end(), array_data.begin(), array_data.end());

  for (auto& stake_change : validators_stakes_changes_) {
    auto stake_change_encoded = util::EncodingSolidity::pack(stake_change.addr_, stake_change.stake_change_);
    result.insert(result.end(), stake_change_encoded.begin(), stake_change_encoded.end());
  }

  return result;
}

PillarBlock PillarBlock::decodeSolidity(const bytes& enc) {
  PillarBlock pillar_block;
  // TODO: implement solidity decoding

  return pillar_block;
}

RLP_FIELDS_DEFINE(PillarBlock, period_, state_root_, bridge_root_, previous_pillar_block_hash_,
                  validators_stakes_changes_)

PillarBlockData::PillarBlockData(std::shared_ptr<PillarBlock> block,
                                 std::vector<std::shared_ptr<PillarVote>>&& pillar_votes)
    : block_(std::move(block)), pillar_votes_(std::move(pillar_votes)) {}
PillarBlockData::PillarBlockData(const dev::RLP& rlp) {
  if (rlp.itemCount() != kRlpItemCount) {
    throw std::runtime_error("PillarBlockData invalid itemCount: " + std::to_string(rlp.itemCount()));
  }

  block_ = std::make_shared<PillarBlock>(rlp[0]);
  pillar_votes_ = decodePillarVotesBundleRlp(rlp[1]);
}

dev::bytes PillarBlockData::getRlp() const {
  dev::RLPStream s(kRlpItemCount);
  s.appendRaw(block_->getRlp());
  s.appendRaw(encodePillarVotesBundleRlp(pillar_votes_));

  return s.invalidate();
}

}  // namespace taraxa::pillar_chain
