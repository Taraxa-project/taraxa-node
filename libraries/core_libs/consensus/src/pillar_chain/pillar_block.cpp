#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"
#include "common/encoding_solidity.hpp"
#include "vote/pillar_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::pillar_chain {

PillarBlock::ValidatorVoteCountChange::ValidatorVoteCountChange(addr_t addr, int32_t vote_count_change)
    : addr_(addr), vote_count_change_(vote_count_change) {}

RLP_FIELDS_DEFINE(PillarBlock::ValidatorVoteCountChange, addr_, vote_count_change_)

PillarBlock::ValidatorVoteCountChange::ValidatorVoteCountChange(const dev::RLP& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), addr_, vote_count_change_);
}

PillarBlock::PillarBlock(const dev::RLP& rlp) : PillarBlock(util::rlp_dec<PillarBlock>(rlp)) {}

PillarBlock::PillarBlock(PbftPeriod period, h256 state_root, blk_hash_t previous_pillar_block_hash, h256 bridge_root,
                         u256 epoch, std::vector<ValidatorVoteCountChange>&& validator_votes_count_changes)
    : pbft_period_(period),
      state_root_(state_root),
      previous_pillar_block_hash_(previous_pillar_block_hash),
      bridge_root_(bridge_root),
      epoch_(epoch),
      validators_votes_count_changes_(std::move(validator_votes_count_changes)) {}

PillarBlock::PillarBlock(const PillarBlock& pillar_block)
    : pbft_period_(pillar_block.pbft_period_),
      state_root_(pillar_block.state_root_),
      previous_pillar_block_hash_(pillar_block.previous_pillar_block_hash_),
      bridge_root_(pillar_block.bridge_root_),
      epoch_(pillar_block.epoch_),
      validators_votes_count_changes_(pillar_block.validators_votes_count_changes_) {}

dev::bytes PillarBlock::getRlp() const { return util::rlp_enc(*this); }

PbftPeriod PillarBlock::getPeriod() const { return pbft_period_; }

blk_hash_t PillarBlock::getPreviousBlockHash() const { return previous_pillar_block_hash_; }

const std::vector<PillarBlock::ValidatorVoteCountChange>& PillarBlock::getValidatorsVoteCountsChanges() const {
  return validators_votes_count_changes_;
}

const h256& PillarBlock::getStateRoot() const { return state_root_; }

const h256& PillarBlock::getBridgeRoot() const { return bridge_root_; }

const u256& PillarBlock::getEpoch() const { return epoch_; }

blk_hash_t PillarBlock::getHash() const {
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
  res["pbft_period"] = static_cast<Json::Value::UInt64>(pbft_period_);
  res["state_root"] = dev::toJS(state_root_);
  res["previous_pillar_block_hash"] = dev::toJS(previous_pillar_block_hash_);
  res["bridge_root"] = dev::toJS(bridge_root_);
  res["epoch"] = dev::toJS(epoch_);
  res["validators_vote_counts_changes"] = Json::Value(Json::arrayValue);
  for (auto const& vote_count_change : validators_votes_count_changes_) {
    Json::Value vote_count_change_json;
    vote_count_change_json["address"] = dev::toJS(vote_count_change.addr_);
    vote_count_change_json["value"] = static_cast<Json::Value::Int64>(vote_count_change.vote_count_change_);

    res["validators_vote_counts_changes"].append(std::move(vote_count_change_json));
  }
  res["hash"] = dev::toJS(getHash());

  return res;
}

dev::bytes PillarBlock::encodeSolidity() const {
  dev::bytes result;
  result.reserve((util::EncodingSolidity::kStartPrefixSize + kFieldsSize + kArrayPosAndSize +
                  (kFieldsInVoteCount * validators_votes_count_changes_.size())) *
                 util::EncodingSolidity::kWordSize);

  auto start = util::EncodingSolidity::pack(util::EncodingSolidity::kStartPrefix);
  result.insert(result.end(), start.begin(), start.end());

  auto body =
      util::EncodingSolidity::pack(pbft_period_, state_root_, previous_pillar_block_hash_, bridge_root_, epoch_);
  result.insert(result.end(), body.begin(), body.end());

  const uint8_t array_position =
      (util::EncodingSolidity::kStartPrefixSize + kFieldsSize) * util::EncodingSolidity::kWordSize;
  auto array_data = util::EncodingSolidity::pack(array_position, validators_votes_count_changes_.size());
  result.insert(result.end(), array_data.begin(), array_data.end());
  for (auto& vote_count_change : validators_votes_count_changes_) {
    auto vote_count_change_encoded =
        util::EncodingSolidity::pack(vote_count_change.addr_, vote_count_change.vote_count_change_);
    result.insert(result.end(), vote_count_change_encoded.begin(), vote_count_change_encoded.end());
  }

  return result;
}

PillarBlock PillarBlock::decodeSolidity(const bytes& enc) {
  PillarBlock b;

  uint64_t start_prefix = 0;
  util::EncodingSolidity::staticUnpack(enc, start_prefix, b.pbft_period_, b.state_root_, b.previous_pillar_block_hash_,
                                       b.bridge_root_, b.epoch_);

  uint64_t array_pos = (util::EncodingSolidity::kStartPrefixSize + kFieldsSize) * util::EncodingSolidity::kWordSize;
  uint64_t array_size = 0;
  bytes array_data(enc.begin() + array_pos, enc.end());
  util::EncodingSolidity::staticUnpack(array_data, array_pos, array_size);
  array_data = bytes(array_data.begin() + kFieldsInVoteCount * util::EncodingSolidity::kWordSize, array_data.end());

  for (size_t i = 0; i < array_size; i++) {
    addr_t addr;
    int64_t vote_count_change;
    util::EncodingSolidity::staticUnpack(array_data, addr, vote_count_change);

    b.validators_votes_count_changes_.emplace_back(addr, vote_count_change);
    array_data = bytes(array_data.begin() + 2 * util::EncodingSolidity::kWordSize, array_data.end());
  }

  return b;
}

RLP_FIELDS_DEFINE(PillarBlock, pbft_period_, state_root_, previous_pillar_block_hash_, bridge_root_, epoch_,
                  validators_votes_count_changes_)

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

Json::Value PillarBlockData::getJson(bool include_signatures) const {
  Json::Value res;
  res["pillar_block"] = block_->getJson();

  if (include_signatures) {
    res["signatures"] = Json::Value(Json::arrayValue);
    for (const auto& vote : pillar_votes_) {
      const auto compact = dev::CompactSignatureStruct(vote->getVoteSignature());

      Json::Value signature;
      signature["r"] = dev::toJS(compact.r);
      signature["vs"] = dev::toJS(compact.vs);

      res["signatures"].append(signature);
    }
  }

  return res;
}

RLP_FIELDS_DEFINE(CurrentPillarBlockDataDb, pillar_block, vote_counts)

}  // namespace taraxa::pillar_chain
