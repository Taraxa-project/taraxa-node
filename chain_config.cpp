#include "chain_config.hpp"

#include <sstream>

namespace taraxa::chain_config {
using std::stringstream;

LazyVal<ChainConfig> const ChainConfig::Default([] {
  ChainConfig ret;
  ret.dag_genesis_block = DagBlock(string(R"({
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
  ret.replay_protection_service.range = 10;
  ret.final_chain.state.chain_config.disable_block_rewards = true;
  ret.final_chain.state.chain_config.evm_chain_config.eth_chain_config
      .DAOForkBlock = state_api::BlockNumberNIL;
  ret.final_chain.state.chain_config.evm_chain_config.execution_options
      .DisableNonceCheck = true;
  ret.final_chain.state.chain_config.evm_chain_config.execution_options
      .DisableGasFee = true;
  ret.final_chain.state
      .genesis_accounts[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")]
      .Balance = 9007199254740991;
  return ret;
});

}  // namespace taraxa::chain_config
