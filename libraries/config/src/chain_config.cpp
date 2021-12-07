#include "config/chain_config.hpp"

#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <libdevcore/CommonJS.h>

#include <sstream>

namespace taraxa::chain_config {
using std::stringstream;

Json::Value enc_json(ChainConfig const& obj) {
  Json::Value json(Json::objectValue);
  if (obj.chain_id) {
    json["chain_id"] = dev::toJS(obj.chain_id);
  }
  json["dag_genesis_block"] = obj.dag_genesis_block.getJson(false);
  json["sortition"] = enc_json(obj.sortition);
  json["pbft"] = enc_json(obj.pbft);
  json["final_chain"] = enc_json(obj.final_chain);
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
}

decltype(ChainConfig::predefined_) const ChainConfig::predefined_([] {
  decltype(ChainConfig::predefined_)::val_t cfgs;

  std::stringstream genesis_sstr(R"({
      "level": 0,
      "tips": [],
      "trxs": [],
      "rewards_votes": [],
      "sig": "b7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
      "hash": "c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69",
      "sender": "de2b1203d72d3549ee2f733b00b2789414c7cea5",
      "pivot": "0000000000000000000000000000000000000000000000000000000000000000",
      "timestamp": 1564617600,
      "vdf": ""
  })");
  Json::Value default_genesis_block_json;
  genesis_sstr >> default_genesis_block_json;

  ChainConfig default_cfg;
  default_cfg.dag_genesis_block = DagBlock(default_genesis_block_json);
  default_cfg.final_chain.state.disable_block_rewards = true;
  default_cfg.final_chain.state.eth_chain_config.dao_fork_block = state_api::BlockNumberNIL;
  default_cfg.final_chain.state.execution_options.disable_nonce_check = true;
  default_cfg.final_chain.state.execution_options.disable_gas_fee = true;
  default_cfg.final_chain.state.execution_options.disable_stats_rewards = true;
  addr_t root_node_addr("de2b1203d72d3549ee2f733b00b2789414c7cea5");
  default_cfg.final_chain.state.genesis_balances[root_node_addr] = 9007199254740991;
  auto& dpos = default_cfg.final_chain.state.dpos.emplace();
  dpos.eligibility_balance_threshold = 1000000000;
  dpos.genesis_state[root_node_addr][root_node_addr] = dpos.eligibility_balance_threshold;
  // VDF config
  default_cfg.sortition.vrf.threshold_upper = 0x8000;
  default_cfg.sortition.vrf.threshold_range = 0xE00;
  default_cfg.sortition.vdf.difficulty_min = 16;
  default_cfg.sortition.vdf.difficulty_max = 21;
  default_cfg.sortition.vdf.difficulty_stale = 22;
  default_cfg.sortition.vdf.lambda_bound = 100;
  // PBFT config
  default_cfg.pbft.lambda_ms_min = 2000;
  default_cfg.pbft.committee_size = 5;
  default_cfg.pbft.dag_blocks_size = 100;
  default_cfg.pbft.ghost_path_move_back = 1;
  default_cfg.pbft.run_count_votes = false;

  cfgs["default"] = std::move(default_cfg);

  return cfgs;
});

}  // namespace taraxa::chain_config
