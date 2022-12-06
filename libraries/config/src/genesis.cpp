#include "config/genesis.hpp"

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
  json["minimum_price"] = obj.minimum_price;
  return json;
}

void dec_json(Json::Value const& json, GasPriceConfig& obj) {
  obj.percentile = json["percentile"].asUInt64();
  obj.blocks = json["blocks"].asUInt64();
  obj.minimum_price = json["minimum_price"].asUInt64();
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
  s << minimum_price;

  return s.out();
}

Json::Value enc_json(const Genesis& obj) {
  Json::Value json(Json::objectValue);
  json["chain_id"] = obj.chain_id;
  json["dag_genesis_block"] = obj.dag_genesis_block.getJson(false);
  json["sortition"] = enc_json(obj.sortition);
  json["pbft"] = enc_json(obj.pbft);
  append_json(json, obj.state);
  json["gas_price"] = enc_json(obj.gas_price);
  json["dag"] = enc_json(obj.dag);
  return json;
}

void dec_json(Json::Value const& json, Genesis& obj) {
  obj.chain_id = json["chain_id"].asUInt();
  obj.dag_genesis_block = DagBlock(json["dag_genesis_block"]);
  dec_json(json["sortition"], obj.sortition);
  dec_json(json["pbft"], obj.pbft);
  dec_json(json, obj.state);
  dec_json(json["gas_price"], obj.gas_price);
  dec_json(json["dag"], obj.dag);
}

Genesis::Genesis() {
  dag_genesis_block = DagBlock(string(R"({
    "level": 0,
    "tips": [],
    "trxs": [],
    "sig": "b7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
    "hash": "c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69",
    "sender": "de2b1203d72d3549ee2f733b00b2789414c7cea5",
    "pivot": "0000000000000000000000000000000000000000000000000000000000000000",
    "timestamp": 1564617600,
    "vdf": ""
  })"));

  // VDF config
  sortition.vrf.threshold_upper = 0xafff;
  sortition.vdf.difficulty_min = 16;
  sortition.vdf.difficulty_max = 21;
  sortition.vdf.difficulty_stale = 23;
  sortition.vdf.lambda_bound = 100;

  // PBFT config
  pbft.lambda_ms = 2000;
  pbft.committee_size = 5;
  pbft.dag_blocks_size = 100;
  pbft.ghost_path_move_back = 1;
  pbft.gas_limit = 60000000;

  // DAG config
  dag.gas_limit = 10000000;

  // DPOS config
  auto& dpos = state.dpos;
  dpos.eligibility_balance_threshold = 1000000000;
  dpos.vote_eligibility_balance_step = 1000000000;
  dpos.validator_maximum_stake = dev::jsToU256("0x84595161401484A000000");
  dpos.yield_percentage = 20;

  uint64_t year_ms = 365 * 24 * 60 * 60;
  year_ms *= 1000;
  // we have fixed 2*lambda time for proposing step and adding some expecting value for filter and certify steps
  const uint32_t expected_block_time = 2 * pbft.lambda_ms + 700;
  dpos.blocks_per_year = year_ms / expected_block_time;
}

void Genesis::validate() const { gas_price.validate(); }

bytes Genesis::rlp() const {
  dev::RLPStream s;
  s.appendList(6);

  s << dag_genesis_block.rlp(true);
  s << gas_price.rlp();
  s << sortition.rlp();
  s << pbft.rlp();
  state.rlp(s);
  s << dag.rlp();

  return s.out();
}

blk_hash_t Genesis::genesisHash() const { return dev::sha3(rlp()); }

}  // namespace taraxa
