#include "chain_config.hpp"

#include <sstream>

namespace taraxa::chain_config {
using std::stringstream;

Json::Value enc_json(ChainConfig const& obj) {
  Json::Value json(Json::objectValue);
  if (obj.chain_id) {
    json["chain_id"] = dev::toJS(obj.chain_id);
  }
  json["dag_genesis_block"] = obj.dag_genesis_block.getJson();
  json["replay_protection_service"] = enc_json(obj.replay_protection_service);
  json["vdf"] = enc_json(obj.vdf);
  json["pbft"] = enc_json(obj.pbft);
  json["final_chain"] = enc_json(obj.final_chain);
  return json;
}

void dec_json(Json::Value const& json, ChainConfig& obj) {
  if (auto const& e = json["chain_id"]; e.isString()) {
    obj.chain_id = dev::jsToInt(e.asString());
  }
  obj.dag_genesis_block = DagBlock(json["dag_genesis_block"]);
  dec_json(json["replay_protection_service"], obj.replay_protection_service);
  dec_json(json["vdf"], obj.vdf);
  dec_json(json["pbft"], obj.pbft);
  dec_json(json["final_chain"], obj.final_chain);
}

decltype(ChainConfig::predefined_) const ChainConfig::predefined_([] {
  decltype(ChainConfig::predefined_)::val_t cfgs;
  cfgs["default"] = [] {
    ChainConfig cfg;
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
    cfg.replay_protection_service.range = 10;
    cfg.final_chain.state.disable_block_rewards = true;
    cfg.final_chain.state.eth_chain_config.dao_fork_block =
        state_api::BlockNumberNIL;
    cfg.final_chain.state.execution_options.disable_nonce_check = true;
    cfg.final_chain.state.execution_options.disable_gas_fee = true;
    addr_t root_node_addr("de2b1203d72d3549ee2f733b00b2789414c7cea5");
    cfg.final_chain.state.genesis_balances[root_node_addr] = 9007199254740991;
    auto& dpos = cfg.final_chain.state.dpos.emplace();
    dpos.eligibility_balance_threshold = 1000000000;
    dpos.genesis_state[root_node_addr][root_node_addr] =
        dpos.eligibility_balance_threshold;
    // VDF config
    cfg.vdf.difficulty_selection = 128;
    cfg.vdf.difficulty_min = 15;
    cfg.vdf.difficulty_max = 21;
    cfg.vdf.difficulty_stale = 22;
    cfg.vdf.lambda_bound = 1500;
    // PBFT config
    cfg.pbft.lambda_ms_min = 2000;
    cfg.pbft.committee_size = 5;
    cfg.pbft.dag_blocks_size = 100;
    cfg.pbft.ghost_path_move_back = 1;
    cfg.pbft.run_count_votes = false;
    return cfg;
  }();
  cfgs["test"] = [&] {
    auto cfg = cfgs["default"];
    cfg.chain_id = 12345;
    cfg.final_chain.state
        .genesis_balances[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")] =
        u256(7200999050) *
        10000000000000000;  // https://ethereum.stackexchange.com/a/74832
    return cfg;
  }();
  return cfgs;
});

}  // namespace taraxa::chain_config
