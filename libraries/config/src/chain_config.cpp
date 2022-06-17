#include "config/chain_config.hpp"

#include <libdevcore/CommonJS.h>

#include <sstream>

#include "common/config_exception.hpp"
#include "libdevcore/SHA3.h"

namespace taraxa {
using std::stringstream;

Json::Value enc_json(GasPriceConfig const& obj) {
  Json::Value json(Json::objectValue);
  json["percentile"] = obj.percentile;
  json["blocks"] = obj.blocks;
  return json;
}

void dec_json(Json::Value const& json, GasPriceConfig& obj) {
  obj.percentile = json["percentile"].asUInt64();
  obj.blocks = json["blocks"].asUInt64();
}

void GasPriceConfig::validate() const {
  if (percentile > 100) {
    throw ConfigException("gas_price::percentile can not be greater then 100");
  }
}

bytes GasPriceConfig::rlp() const {
  dev::RLPStream s;
  s.appendList(2);

  s << percentile;
  s << blocks;

  return s.out();
}

Json::Value enc_json(const ChainConfig& obj) {
  Json::Value json(Json::objectValue);
  if (obj.chain_id) {
    json["chain_id"] = dev::toJS(obj.chain_id);
  }
  json["dag_genesis_block"] = obj.dag_genesis_block.getJson(false);
  json["sortition"] = enc_json(obj.sortition);
  json["pbft"] = enc_json(obj.pbft);
  json["final_chain"] = enc_json(obj.final_chain);
  json["gas_price"] = enc_json(obj.gas_price);
  json["dag"] = enc_json(obj.dag);
  return json;
}

void dec_json(Json::Value const& json, ChainConfig& obj) {
  if (auto const& e = json["chain_id"]; e.isString()) {
    obj.chain_id = dev::jsToInt(e.asString());
  }
  obj.dag_genesis_block = DagBlock(json["dag_genesis_block"]);
  dec_json(json["sortition"], obj.sortition);
  dec_json(json["pbft"], obj.pbft);
  dec_json(json["final_chain"], obj.final_chain);
  dec_json(json["gas_price"], obj.gas_price);
  dec_json(json["dag"], obj.dag);
}

void ChainConfig::validate() const { gas_price.validate(); }

bytes ChainConfig::rlp() const {
  dev::RLPStream s;
  s.appendList(7);

  s << chain_id;
  s << dag_genesis_block.rlp(true);
  s << gas_price.rlp();
  s << sortition.rlp();
  s << pbft.rlp();
  s << final_chain.rlp();
  s << dag.rlp();

  return s.out();
}

blk_hash_t ChainConfig::genesisHash() const { return dev::sha3(rlp()); }

}  // namespace taraxa
