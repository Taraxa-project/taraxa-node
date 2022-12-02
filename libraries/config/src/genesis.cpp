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
  json["dag_genesis_block"] = obj.dag_genesis_block.getJson(false);
  json["sortition"] = enc_json(obj.sortition);
  json["pbft"] = enc_json(obj.pbft);
  json["state"] = enc_json(obj.state);
  json["gas_price"] = enc_json(obj.gas_price);
  json["dag"] = enc_json(obj.dag);
  return json;
}

void dec_json(Json::Value const& json, Genesis& obj) {
  obj.dag_genesis_block = DagBlock(json["dag_genesis_block"]);
  dec_json(json["sortition"], obj.sortition);
  dec_json(json["pbft"], obj.pbft);
  dec_json(json["state"], obj.state);
  dec_json(json["gas_price"], obj.gas_price);
  dec_json(json["dag"], obj.dag);
}

const Genesis& Genesis::predefined(std::string const& name) {
  if (auto i = predefined_->find(name); i != predefined_->end()) {
    return i->second;
  }
  throw std::runtime_error("unknown chain config: " + name);
}

decltype(Genesis::predefined_) const Genesis::predefined_([] {
  decltype(Genesis::predefined_)::val_t cfgs;
  cfgs["default"] = [] {
    Genesis cfg;
    cfg.dag_genesis_block = DagBlock(string(R"({
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
    cfg.sortition.vrf.threshold_upper = 0xafff;
    cfg.sortition.vdf.difficulty_min = 16;
    cfg.sortition.vdf.difficulty_max = 21;
    cfg.sortition.vdf.difficulty_stale = 23;
    cfg.sortition.vdf.lambda_bound = 100;

    // PBFT config
    cfg.pbft.lambda_ms = 2000;
    cfg.pbft.committee_size = 5;
    cfg.pbft.dag_blocks_size = 100;
    cfg.pbft.ghost_path_move_back = 1;
    cfg.pbft.gas_limit = 60000000;

    // DAG config
    cfg.dag.gas_limit = 10000000;

    // DPOS config
    auto& dpos = cfg.state.dpos;
    dpos.eligibility_balance_threshold = 1000000000;
    dpos.vote_eligibility_balance_step = 1000000000;
    dpos.validator_maximum_stake = dev::jsToU256("0x84595161401484A000000");
    // dpos.yield_percentage = 0;
    dpos.yield_percentage = 20;

    uint64_t year_ms = 365 * 24 * 60 * 60;
    year_ms *= 1000;
    // we have fixed 2*lambda time for proposing step and adding some expecting value for filter and certify steps
    const uint32_t expected_block_time = 2 * cfg.pbft.lambda_ms + 700;
    dpos.blocks_per_year = year_ms / expected_block_time;

    return cfg;
  }();
  cfgs["test"] = [&] {
    auto cfg = cfgs["default"];
    cfg.state.dpos.yield_percentage = 0;
    cfg.gas_price.minimum_price = 0;
    cfg.state.initial_balances[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")] =
        u256(7200999050) * 10000000000000000;  // https://ethereum.stackexchange.com/a/74832
    return cfg;
  }();
  return cfgs;
});

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
