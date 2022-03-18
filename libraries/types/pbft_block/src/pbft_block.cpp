#include "pbft/pbft_block.hpp"

#include <libdevcore/CommonJS.h>

#include <iostream>

#include "common/jsoncpp.hpp"

namespace taraxa {
PbftBlock::PbftBlock(bytes const& b) : PbftBlock(dev::RLP(b)) {}

PbftBlock::PbftBlock(dev::RLP const& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), prev_block_hash_, dag_block_hash_as_pivot_, order_hash_, period_,
                  timestamp_, signature_);
  calculateHash_();
}

PbftBlock::PbftBlock(blk_hash_t const& prev_blk_hash, blk_hash_t const& dag_blk_hash_as_pivot,
                     blk_hash_t const& order_hash, uint64_t period, addr_t const& beneficiary, secret_t const& sk)
    : prev_block_hash_(prev_blk_hash),
      dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
      order_hash_(order_hash),
      period_(period),
      beneficiary_(beneficiary) {
  timestamp_ = dev::utcTime();
  signature_ = dev::sign(sk, sha3(false));
  calculateHash_();
}

PbftBlock::PbftBlock(std::string const& str) {
  auto doc = util::parse_json(str);
  block_hash_ = blk_hash_t(doc["block_hash"].asString());
  prev_block_hash_ = blk_hash_t(doc["prev_block_hash"].asString());
  dag_block_hash_as_pivot_ = blk_hash_t(doc["dag_block_hash_as_pivot"].asString());
  order_hash_ = blk_hash_t(doc["order_hash"].asString());
  period_ = doc["period"].asUInt64();
  timestamp_ = doc["timestamp"].asUInt64();
  signature_ = sig_t(doc["signature"].asString());
  beneficiary_ = addr_t(doc["beneficiary"].asString());
}

Json::Value PbftBlock::toJson(PbftBlock const& b, std::vector<blk_hash_t> const& dag_blks) {
  auto ret = b.getJson();
  // Legacy schema
  auto& schedule_json = ret["schedule"] = Json::Value(Json::objectValue);
  auto& dag_blks_json = schedule_json["dag_blocks_order"] = Json::Value(Json::arrayValue);
  for (auto const& h : dag_blks) {
    dag_blks_json.append(dev::toJS(h));
  }
  return ret;
}

void PbftBlock::calculateHash_() {
  if (!block_hash_) {
    block_hash_ = dev::sha3(rlp(true));
  } else {
    // Hash sould only be calculated once
    assert(false);
  }
  auto p = dev::recover(signature_, sha3(false));
  assert(p);
  beneficiary_ = dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
}

blk_hash_t PbftBlock::sha3(bool include_sig) const { return dev::sha3(rlp(include_sig)); }

std::string PbftBlock::getJsonStr() const { return getJson().toStyledString(); }

Json::Value PbftBlock::getJson() const {
  Json::Value json;
  json["prev_block_hash"] = prev_block_hash_.toString();
  json["dag_block_hash_as_pivot"] = dag_block_hash_as_pivot_.toString();
  json["order_hash"] = order_hash_.toString();
  json["period"] = (Json::Value::UInt64)period_;
  json["timestamp"] = (Json::Value::UInt64)timestamp_;
  json["block_hash"] = block_hash_.toString();
  json["signature"] = signature_.toString();
  json["beneficiary"] = beneficiary_.toString();
  return json;
}

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm, bool include_sig) const {
  strm.appendList(include_sig ? 6 : 5);
  strm << prev_block_hash_;
  strm << dag_block_hash_as_pivot_;
  strm << order_hash_;
  strm << period_;
  strm << timestamp_;
  if (include_sig) {
    strm << signature_;
  }
}

bytes PbftBlock::rlp(bool include_sig) const {
  dev::RLPStream strm;
  streamRLP(strm, include_sig);
  return strm.invalidate();
}

std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk) {
  strm << pbft_blk.getJsonStr();
  return strm;
}

}  // namespace taraxa