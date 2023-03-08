#include <gtest/gtest.h>
#include <libdevcore/Address.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include "network/rpc/eth/Eth.h"
#include "test_util/gtest.hpp"
#include "test_util/samples.hpp"

namespace taraxa::core_tests {

struct RPCTest : NodesTest {};

TEST_F(RPCTest, eth_estimateGas) {
  auto node_cfg = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfg);
  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = node_cfg.front().genesis.chain_id;
  eth_rpc_params.gas_limit = node_cfg.front().genesis.dag.gas_limit;
  eth_rpc_params.final_chain = nodes.front()->getFinalChain();
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  const auto from = dev::toHex(dev::toAddress(node_cfg.front().node_secret));
  // Contract creation estimations with author + without author
  {
    Json::Value trx(Json::objectValue);
    trx["data"] = samples::greeter_contract_code;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5ccc5");
    trx["from"] = from;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5ccc5");
  }

  // Contract creation with value
  {
    Json::Value trx(Json::objectValue);
    trx["value"] = 1;
    trx["data"] = samples::greeter_contract_code;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5ccc5");
  }

  // Simple transfer estimations with author + without author
  {
    Json::Value trx(Json::objectValue);
    trx["value"] = 1;
    trx["to"] = dev::toHex(addr_t::random());
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5208");  // 21k
    trx["from"] = from;
    EXPECT_EQ(eth_json_rpc->eth_estimateGas(trx), "0x5208");  // 21k
  }

  // Test throw on failed transaction
  {
    Json::Value trx(Json::objectValue);
    trx["value"] = 1000;
    trx["to"] = dev::toHex(addr_t::random());
    trx["from"] = dev::toHex(addr_t::random());
    EXPECT_THROW(eth_json_rpc->eth_estimateGas(trx), std::exception);
  }
}

#define EXPECT_THROW_WITH(statement, expected_exception, msg) \
  try {                                                       \
    statement;                                                \
  } catch (const expected_exception& e) {                     \
    ASSERT_EQ(std::string(msg), std::string(e.what()));       \
  }

TEST_F(RPCTest, eth_call) {
  auto node_cfg = make_node_cfgs(1);
  auto nodes = launch_nodes(node_cfg);
  const auto final_chain = nodes.front()->getFinalChain();

  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = node_cfg.front().genesis.chain_id;
  eth_rpc_params.gas_limit = node_cfg.front().genesis.dag.gas_limit;
  eth_rpc_params.final_chain = final_chain;
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  const auto last_block_num = final_chain->last_block_number();
  const u256 total_eligible = final_chain->dpos_eligible_total_vote_count(last_block_num);
  const auto total_eligible_str = dev::toHexPrefixed(dev::toBigEndian(total_eligible));

  const auto empty_address = dev::KeyPair::create().address().toString();
  // check that balance of empty_address is 0
  ASSERT_EQ(eth_json_rpc->eth_getBalance(empty_address, dev::toCompactHexPrefixed(last_block_num)), "0x0");

  const std::string get_total_eligible_method("0xde8e4b50");
  const auto dpos_contract("0x00000000000000000000000000000000000000FE");
  {
    // Tested on Ethereum mainnet node. OK.
    // Sending some value to random address(not contract). from == zeroAddress
    // '{"method":"eth_call","params":[{"to":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","value":"0x100"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["to"] = empty_address;
    trx["value"] = "0x100";
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), "0x");
  }

  {
    // Tested on Ethereum mainnet node.
    // ERROR: insufficient funds for gas * price + value.
    // Sending some value from account with no funds to other address.
    // '{"method":"eth_call","params":[{"from":"0x46dE41a622e679B8CDa5B76942a2e3Df5Ba023db","to":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","value":"0x100"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dev::KeyPair::create().address().toString();
    trx["value"] = "0x100";
    EXPECT_THROW_WITH(eth_json_rpc->eth_call(trx, "latest"), std::runtime_error, "insufficient balance for transfer");
  }

  {
    // Tested on Ethereum mainnet node. OK
    // '{"method":"eth_call","params":[{"to":"0xdac17f958d2ee523a2206206994597c13d831ec7","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["to"] = dpos_contract;
    trx["data"] = get_total_eligible_method;
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), total_eligible_str);
  }

  {
    // Tested on Ethereum mainnet node. OK
    // '{"method":"eth_call","params":[{"to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x100000","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["to"] = dpos_contract;
    trx["gas"] = "0x100000";
    trx["data"] = get_total_eligible_method;
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), total_eligible_str);
  }

  {
    // Tested on Ethereum mainnet node. OK.
    // gas * gasPrice balance check eliminated for `from == ZeroAddress`.
    // '{"method":"eth_call","params":[{"to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x100000","gasPrice":"0x241268485270","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["to"] = dpos_contract;
    trx["gas"] = "0x100000";
    trx["gasPrice"] = "0x241268485270";
    trx["data"] = get_total_eligible_method;
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), total_eligible_str);
  }

  {
    // Tested on Ethereum mainnet node.
    // ERROR: insufficient funds for gas * price + value.
    // Sending from address with no funds, so can't pay gas * gas_price
    // '{"method":"eth_call","params":[{"from":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x100000","gasPrice":"0x241268485270","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dpos_contract;
    trx["gas"] = "0x100000";
    trx["gasPrice"] = "0x241268485270";
    trx["data"] = get_total_eligible_method;
    EXPECT_THROW_WITH(eth_json_rpc->eth_call(trx, "latest"), std::runtime_error, "insufficient balance to pay for gas");
  }

  {
    // Tested on Ethereum mainnet node. OK.
    // Sending from address with no funds. Default `gasPrice == 0`, no funds needed
    // '{"method":"eth_call","params":[{"from":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","to":"0xdac17f958d2ee523a2206206994597c13d831ec7","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dpos_contract;
    trx["data"] = get_total_eligible_method;
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), total_eligible_str);
  }

  {
    // Tested on Ethereum mainnet node. OK.
    // Sending from address with no funds. Default `gasPrice == 0`, no funds needed. Custom sufficient gas value is ok
    // '{"method":"eth_call","params":[{"from":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x100000","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dpos_contract;
    trx["gas"] = "0x100000";
    trx["data"] = get_total_eligible_method;
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), total_eligible_str);
  }

  {
    // Tested on Ethereum mainnet node.
    // ERROR: intrinsic gas too low
    // Sending from address with no funds. Default `gasPrice == 0`, no funds needed. Gas value lower then intrinsic gas
    // '{"method":"eth_call","params":[{"from":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x1000","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dpos_contract;
    trx["gas"] = "0x1000";
    trx["data"] = get_total_eligible_method;
    EXPECT_THROW_WITH(eth_json_rpc->eth_call(trx, "latest"), std::runtime_error, "intrinsic gas too low");
  }

  {
    // Tested on Ethereum mainnet node.
    // ERROR: out of gas
    // Sending from address with no funds. Default `gasPrice == 0`, no funds needed.
    // Gas value lower then needed intrinsic gas, but lower then total required gas
    // '{"method":"eth_call","params":[{"from":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x5250","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dpos_contract;
    trx["gas"] = "0x5330";
    trx["data"] = get_total_eligible_method;
    EXPECT_THROW_WITH(eth_json_rpc->eth_call(trx, "latest"), std::runtime_error, "out of gas");
  }

  {
    // Tested on Ethereum mainnet node. OK.
    // Sending from address with no funds. default `gasPrice == 0`, so no funds needed. Ok with custom gas value
    // '{"method":"eth_call","params":[{"from":"0xeEA2524616B61E12c0Cb00a41dA78Ded1635F566","to":"0xdac17f958d2ee523a2206206994597c13d831ec7","gas":"0x100000","data":"0x3eaaf86b"},"latest"]}'
    Json::Value trx(Json::objectValue);
    trx["from"] = empty_address;
    trx["to"] = dpos_contract;
    trx["gas"] = "0x100000";
    trx["data"] = get_total_eligible_method;
    EXPECT_EQ(eth_json_rpc->eth_call(trx, "latest"), total_eligible_str);
  }
}

TEST_F(RPCTest, eth_getBlock) {
  auto node_cfg = make_node_cfgs(1, 1, 10);
  // Enable rewards distribution
  node_cfg[0].genesis.state.dpos.yield_percentage = 10;
  auto nodes = launch_nodes(node_cfg);
  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = node_cfg.front().genesis.chain_id;
  eth_rpc_params.gas_limit = node_cfg.front().genesis.dag.gas_limit;
  eth_rpc_params.final_chain = nodes.front()->getFinalChain();
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  wait({10s, 500ms}, [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, 5, nodes[0]->getFinalChain()->last_block_number()); });
  auto block = eth_json_rpc->eth_getBlockByNumber("0x4", false);

  EXPECT_EQ(4, dev::jsToU256(block["number"].asString()));
  EXPECT_GT(dev::jsToU256(block["totalReward"].asString()), 0);
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