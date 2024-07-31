#include "pbft/pbft_block_extra_data.hpp"

namespace taraxa {

PbftBlockExtraData::PbftBlockExtraData(const uint16_t major_version, const uint16_t minor_version,
                                       const uint16_t patch_version, const uint16_t net_version,
                                       const std::string node_implementation,
                                       const std::optional<blk_hash_t>& pillar_block_hash)
    : major_version_(major_version),
      minor_version_(minor_version),
      patch_version_(patch_version),
      net_version_(net_version),
      node_implementation_(node_implementation),
      pillar_block_hash_(pillar_block_hash) {}

std::optional<PbftBlockExtraData> PbftBlockExtraData::fromBytes(const bytes& data) {
  if (data.size() > kExtraDataMaxSize) {
    throw std::runtime_error("Pbft block invalid, extra data size over the limit");
  }

  dev::RLP rlp(data);
  PbftBlockExtraData extra_data;
  try {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), extra_data.major_version_, extra_data.minor_version_,
                    extra_data.patch_version_, extra_data.net_version_, extra_data.node_implementation_,
                    extra_data.pillar_block_hash_);
  } catch (const dev::RLPException& e) {
    return std::nullopt;
  }

  return extra_data;
}

bytes PbftBlockExtraData::rlp() const {
  dev::RLPStream s;
  util::rlp_tuple(s, major_version_, minor_version_, patch_version_, net_version_, node_implementation_,
                  pillar_block_hash_);

  return s.invalidate();
}

Json::Value PbftBlockExtraData::getJson() const {
  Json::Value json;
  json["major_version"] = major_version_;
  json["minor_version"] = minor_version_;
  json["patch_version"] = patch_version_;
  json["net_version"] = net_version_;
  json["node_implementation"] = node_implementation_;
  json["pillar_block_hash"] = pillar_block_hash_.has_value() ? pillar_block_hash_->toString() : "";

  return json;
}

std::optional<blk_hash_t> PbftBlockExtraData::getPillarBlockHash() const { return pillar_block_hash_; }

}  // namespace taraxa