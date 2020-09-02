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
    cfg.final_chain.state.chain_config.disable_block_rewards = true;
    cfg.final_chain.state.chain_config.evm_chain_config.eth_chain_config
        .dao_fork_block = state_api::BlockNumberNIL;
    cfg.final_chain.state.chain_config.evm_chain_config.execution_options
        .disable_nonce_check = true;
    cfg.final_chain.state.chain_config.evm_chain_config.execution_options
        .disable_gas_fee = true;
    cfg.final_chain.state
        .genesis_accounts[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")]
        .Balance = 9007199254740991;
    return cfg;
  }();
  cfgs["test"] = [&] {
    auto cfg = cfgs["default"];
    cfg.chain_id = 12345;
    cfg.final_chain.state
        .genesis_accounts[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")]
        .Balance =
        u256(7200999050) *
        10000000000000000;  // https://ethereum.stackexchange.com/a/74832
    return cfg;
  }();
  return cfgs;
});

}  // namespace taraxa::chain_config
