#include "chain_config.hpp"

#include <sstream>

namespace taraxa::chain_config {
using std::stringstream;

LazyVal<ChainConfig> const ChainConfig::Default([] {
  ChainConfig ret;
  ret.chain_id = -4;
  ret.dag_genesis_block.sign(secret_t(
      "b7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f2"
      "5be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701"));
  ret.final_chain.replay_protection_service_range = 10;
  ret.final_chain.state.chain_config.DisableBlockRewards = true;
  ret.final_chain.state.chain_config.EVMChainConfig.ETHChainConfig
      .DAOForkBlock = StateAPI::BlockNumberNIL;
  ret.final_chain.state.chain_config.EVMChainConfig.ExecutionOptions
      .DisableNonceCheck = true;
  ret.final_chain.state.chain_config.EVMChainConfig.ExecutionOptions
      .DisableGasFee = true;
  ret.final_chain.state
      .genesis_accounts[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")]
      .Balance = 9007199254740991;
  return ret;
});

}  // namespace taraxa::chain_config
