#include "network/rpc/Taraxa.h"
#include "util/jsoncpp.hpp"
#include "util_test/util.hpp"

namespace taraxa::net {
using namespace core_tests;

struct TaraxaTest : BaseTest {
  vector<FullNode::Handle> nodes = launch_nodes(make_node_cfgs<20, true>(1));
  TransactionClient trx_client{nodes[0]};
  FullNode& node = *nodes[0];
  FullNodeConfig const& cfg = node.getConfig();
  filesystem::path response_file = data_dir / "response.json";
};

TEST_F(TaraxaTest, queryDPOS) {
  auto enough_balance = cfg.chain.final_chain.state.dpos->eligibility_balance_threshold;
  state_api::DPOSTransfers transfers;
  transfers[make_addr(1)].value = enough_balance;
  transfers[make_addr(2)].value = enough_balance - 1;
  transfers[make_addr(3)].value = enough_balance + 1;
  trx_client.must_process_sync(make_dpos_trx(cfg, transfers));
  /*
   * The following is a flat json array of request and expected response pairs,
   * evaluated and checked in the order of occurrence.
   */
  auto req_res = util::parse_json(R"([
  {
    "account_queries": {
      "0x0000000000000000000001000000000000000000": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      },
      "0x0000000000000000000002000000000000000000": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      },
      "0x0000000000000000000003000000000000000000": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      },
      "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      }
    },
    "block_number": "0x0",
    "with_eligible_count": true
  },
  {
    "account_results": {
      "0x0000000000000000000001000000000000000000": {
        "inbound_deposits": [],
        "is_eligible": false,
        "outbound_deposits": [],
        "staking_balance": "0x0"
      },
      "0x0000000000000000000002000000000000000000": {
        "inbound_deposits": [],
        "is_eligible": false,
        "outbound_deposits": [],
        "staking_balance": "0x0"
      },
      "0x0000000000000000000003000000000000000000": {
        "inbound_deposits": [],
        "is_eligible": false,
        "outbound_deposits": [],
        "staking_balance": "0x0"
      },
      "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "inbound_deposits": [
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "is_eligible": true,
        "outbound_deposits": [
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "staking_balance": "0x3b9aca00"
      }
    },
    "eligible_count": "0x1"
  },
  {
    "account_queries": {
      "0x0000000000000000000001000000000000000000": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      },
      "0x0000000000000000000002000000000000000000": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      },
      "0x0000000000000000000003000000000000000000": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      },
      "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "inbound_deposits_addrs_only": true,
        "outbound_deposits_addrs_only": true,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": true
      }
    },
    "block_number": null,
    "with_eligible_count": true
  },
  {
    "account_results": {
      "0x0000000000000000000001000000000000000000": {
        "inbound_deposits": [
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "is_eligible": true,
        "outbound_deposits": [],
        "staking_balance": "0x3b9aca00"
      },
      "0x0000000000000000000002000000000000000000": {
        "inbound_deposits": [
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "is_eligible": false,
        "outbound_deposits": [],
        "staking_balance": "0x3b9ac9ff"
      },
      "0x0000000000000000000003000000000000000000": {
        "inbound_deposits": [
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "is_eligible": true,
        "outbound_deposits": [],
        "staking_balance": "0x3b9aca01"
      },
      "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "inbound_deposits": [
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "is_eligible": true,
        "outbound_deposits": [
          "0x0000000000000000000001000000000000000000",
          "0x0000000000000000000002000000000000000000",
          "0x0000000000000000000003000000000000000000",
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5"
        ],
        "staking_balance": "0x3b9aca00"
      }
    },
    "eligible_count": "0x3"
  },
  {
    "account_queries": {
      "0x0000000000000000000001000000000000000000": {
        "inbound_deposits_addrs_only": false,
        "outbound_deposits_addrs_only": false,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": false
      },
      "0x0000000000000000000002000000000000000000": {
        "inbound_deposits_addrs_only": false,
        "outbound_deposits_addrs_only": false,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": false
      },
      "0x0000000000000000000003000000000000000000": {
        "inbound_deposits_addrs_only": false,
        "outbound_deposits_addrs_only": false,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": false
      },
      "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "inbound_deposits_addrs_only": false,
        "outbound_deposits_addrs_only": false,
        "with_inbound_deposits": true,
        "with_outbound_deposits": true,
        "with_staking_balance": false
      }
    },
    "with_eligible_count": false
  },
  {
    "account_results": {
      "0x0000000000000000000001000000000000000000": {
        "inbound_deposits": {
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x3b9aca00"
        },
        "outbound_deposits": {}
      },
      "0x0000000000000000000002000000000000000000": {
        "inbound_deposits": {
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x3b9ac9ff"
        },
        "outbound_deposits": {}
      },
      "0x0000000000000000000003000000000000000000": {
        "inbound_deposits": {
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x3b9aca01"
        },
        "outbound_deposits": {}
      },
      "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "inbound_deposits": {
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x3b9aca00"
        },
        "outbound_deposits": {
          "0x0000000000000000000001000000000000000000": "0x3b9aca00",
          "0x0000000000000000000002000000000000000000": "0x3b9ac9ff",
          "0x0000000000000000000003000000000000000000": "0x3b9aca01",
          "0xde2b1203d72d3549ee2f733b00b2789414c7cea5": "0x3b9aca00"
        }
      }
    }
  }
])");
  for (uint i = 0, itrs = req_res.size() / 2; i < itrs; ++i) {
    auto const& req = req_res[0 + 2 * i];
    auto const& res_expected = req_res[1 + 2 * i];
    // TODO make/use generic jsonrpc request util. json-rpc-cpp client utils are bugged
    Json::Value req_wrapper(Json::objectValue);
    req_wrapper["jsonrpc"] = "2.0";
    req_wrapper["id"] = "0";
    req_wrapper["method"] = "taraxa_queryDPOS";
    (req_wrapper["params"] = Json::Value(Json::arrayValue)).append(req);
    stringstream curl_cmd;
    curl_cmd << "curl -m 10 -s -X POST -d '" << util::to_string(req_wrapper) << "' 0.0.0.0:" << *cfg.rpc->http_port
             << " >" << response_file;
    EXPECT_FALSE(system(curl_cmd.str().c_str()));
    Json::Value res_wrapper;
    ifstream(response_file) >> res_wrapper;
    EXPECT_EQ(res_expected, res_wrapper["result"]);
  }
}

}  // namespace taraxa::net

TARAXA_TEST_MAIN({})
