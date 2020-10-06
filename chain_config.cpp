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
  json["final_chain"] = enc_json(obj.final_chain);
  return json;
}

void dec_json(Json::Value const& json, ChainConfig& obj) {
  if (auto const& e = json["chain_id"]; e.isString()) {
    obj.chain_id = dev::jsToInt(e.asString());
  }
  obj.dag_genesis_block = DagBlock(json["dag_genesis_block"]);
  dec_json(json["replay_protection_service"], obj.replay_protection_service);
  dec_json(json["final_chain"], obj.final_chain);
}

LazyVal<addr_t> const ChainConfig::default_chain_boot_node_addr([] {
  return addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5");
});

LazyVal<vector<addr_t>> const ChainConfig::default_chain_predefined_nodes([] {
  return vector<addr_t>{
      default_chain_boot_node_addr,
      addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"),
      addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e"),
      addr_t("415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"),
      addr_t("b770f7a99d0b7ad9adf6520be77ca20ee99b0858"),
  };
});

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
    auto& dpos = cfg.final_chain.state.dpos.emplace();
    dpos.eligibility_balance_threshold = 1000000000;
    cfg.final_chain.state.genesis_balances[*default_chain_boot_node_addr] =
        default_chain_boot_node_initial_balance +
        dpos.eligibility_balance_threshold *
            default_chain_predefined_nodes->size();
    for (auto const& addr_str : *default_chain_predefined_nodes) {
      dpos.genesis_state[*default_chain_boot_node_addr][addr_t(addr_str)] =
          dpos.eligibility_balance_threshold;
    }
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
