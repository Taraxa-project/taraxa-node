#include <gtest/gtest.h>
#include <libdevcore/Address.h>
#include <libdevcore/Common.h>

#include "network/rpc/eth/Eth.h"
#include "util_test/gtest.hpp"
#include "util_test/samples.hpp"

namespace taraxa::core_tests {

struct RPCTest : BaseTest {};

TEST_F(RPCTest, eth_estimateGas) {
  auto node_cfg = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfg);
  const auto chain_id = node_cfg.front().chain_id;
  const auto from = dev::toHex(dev::toAddress(node_cfg.front().node_secret));
  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = chain_id;
  eth_rpc_params.final_chain = nodes.front()->getFinalChain();
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));
  // Contract creation estimations with author + without author
  {
    Json::Value trx(Json::objectValue);
    trx["data"] = samples::greeter_contract_code;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5ca85");
    trx["from"] = from;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5ca85");
  }

  // Simple transfer estimations with author + without author
  {
    Json::Value trx(Json::objectValue);
    trx["value"] = 1;
    trx["to"] = dev::toHex(addr_t::random());
    trx["from"] = from;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5208");  // 21k
  }
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();

  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;
  addr_t node_addr;
  logging.InitLogging(node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}