#include "config/dag_config.hpp"

#include <libdevcore/CommonJS.h>
#include <libdevcore/RLP.h>

namespace taraxa {

bytes BlockProposerConfig::rlp() const {
  dev::RLPStream s;
  s.appendList(2);

  s << shard;
  s << transaction_limit;

  return s.out();
}

Json::Value enc_json(const BlockProposerConfig& obj) {
  Json::Value ret(Json::objectValue);
  ret["shard"] = dev::toJS(obj.shard);
  ret["transaction_limit"] = dev::toJS(obj.transaction_limit);
  return ret;
}
void dec_json(const Json::Value& json, BlockProposerConfig& obj) {
  obj.shard = dev::getUInt(json["shard"]);
  obj.transaction_limit = dev::getUInt(json["transaction_limit"]);
}

bytes DagConfig::rlp() const {
  dev::RLPStream s;
  s.appendList(2);

  s << gas_limit;
  s << block_proposer.rlp();

  return s.out();
}

Json::Value enc_json(const DagConfig& obj) {
  Json::Value ret(Json::objectValue);
  ret["block_proposer"] = enc_json(obj.block_proposer);
  ret["gas_limit"] = dev::toJS(obj.gas_limit);
  return ret;
}

void dec_json(const Json::Value& json, DagConfig& obj) {
  dec_json(json["block_proposer"], obj.block_proposer);
  obj.gas_limit = dev::getUInt(json["gas_limit"]);
}

}  // namespace taraxa