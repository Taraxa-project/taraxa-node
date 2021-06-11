#include "data.hpp"

#include <libdevcore/CommonJS.h>

#include "common/constants.hpp"

namespace taraxa::final_chain {

Json::Value enc_json(Config const& obj) {
  Json::Value json(Json::objectValue);
  json["state"] = enc_json(obj.state);
  json["genesis_block_fields"] = enc_json(obj.genesis_block_fields);
  return json;
}

void dec_json(Json::Value const& json, Config& obj) {
  dec_json(json["state"], obj.state);
  dec_json(json["genesis_block_fields"], obj.genesis_block_fields);
}

Json::Value enc_json(Config::GenesisBlockFields const& obj) {
  Json::Value json(Json::objectValue);
  json["timestamp"] = dev::toJS(obj.timestamp);
  json["author"] = dev::toJS(obj.author);
  return json;
}

void dec_json(Json::Value const& json, Config::GenesisBlockFields& obj) {
  obj.timestamp = dev::jsToInt(json["timestamp"].asString());
  obj.author = addr_t(json["author"].asString());
}

RLP_FIELDS_DEFINE(BlockHeader, hash, ethereum_rlp_size, parent_hash, author, state_root, transactions_root,
                  receipts_root, log_bloom, number, gas_limit, gas_used, timestamp, extra_data)

void BlockHeader::ethereum_rlp(dev::RLPStream& encoding) const {
  util::rlp_tuple(encoding, parent_hash, BlockHeader::uncles_hash(), author, state_root, transactions_root,
                  receipts_root, log_bloom, BlockHeader::difficulty(), number, gas_limit, gas_used, timestamp,
                  extra_data, BlockHeader::mix_hash(), BlockHeader::nonce());
}

h256 const& BlockHeader::uncles_hash() { return EmptyRLPListSHA3(); }

Nonce const& BlockHeader::nonce() {
  static const Nonce ret;
  return ret;
}

u256 const& BlockHeader::difficulty() {
  static const u256 ret = 0;
  return ret;
}

h256 const& BlockHeader::mix_hash() {
  static const h256 ret;
  return ret;
}

RLP_FIELDS_DEFINE(LogEntry, address, topics, data)

LogBloom LogEntry::bloom() const {
  LogBloom ret;
  ret.shiftBloom<3>(sha3(address.ref()));
  for (auto t : topics) {
    ret.shiftBloom<3>(sha3(t.ref()));
  }
  return ret;
}

RLP_FIELDS_DEFINE(TransactionReceipt, status_code, gas_used, cumulative_gas_used, logs, new_contract_address)

LogBloom TransactionReceipt::bloom() const {
  LogBloom ret;
  for (auto const& l : logs) {
    ret |= l.bloom();
  }
  return ret;
}

RLP_FIELDS_DEFINE(TransactionLocation, blk_n, index)

}  // namespace taraxa::final_chain