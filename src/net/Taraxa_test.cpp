#include "Taraxa.h"

// in our docker build we use libjsonrpccpp 0.7.0, which
// has the C++17 incompatibility (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0003r5.html)
// in some files that is addressed by the following horrible hack.
// TODO remove it after the lib upgrade (later versions have the root cause fixed).
#define throw(...)
#include <jsonrpccpp/client/connectors/httpclient.h>

#include "TaraxaClient.h"
#undef throw
// END horrible hack

#include "../util_test/util.hpp"

namespace taraxa::net {
using namespace core_tests;

struct TaraxaTest : BaseTest {
  vector<FullNode::Handle> nodes = launch_nodes(make_node_cfgs<20, true>(1));
  TransactionClient trx_client{nodes[0]};
  FullNode& node = *nodes[0];
  FullNodeConfig const& cfg = node.getConfig();
  jsonrpc::HttpClient connector{"http://localhost:" + to_string(*cfg.rpc->http_port)};
  TaraxaClient c{connector};
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
  auto req_res = parse_json(R"([
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
    EXPECT_EQ(res_expected, c.taraxa_queryDPOS(req));
  }
}

}  // namespace taraxa::net

TARAXA_TEST_MAIN({});
