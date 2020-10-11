#ifndef TARAXA_NODE_CORE_TESTS_UTIL_HPP
#define TARAXA_NODE_CORE_TESTS_UTIL_HPP

#include <libdevcrypto/Common.h>

#include <array>
#include <boost/filesystem.hpp>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "../full_node.hpp"
#include "../util/gtest.hpp"
#include "config.hpp"
#include "network.hpp"
#include "transaction_manager.hpp"
#include "util/lazy.hpp"

namespace taraxa::core_tests {
using namespace std;
using namespace std::chrono;
using boost::filesystem::is_regular_file;
using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using boost::filesystem::remove_all;
using dev::KeyPair;
using dev::Secret;
using ::taraxa::util::lazy::Lazy;

inline auto const DIR = path(__FILE__).parent_path();
inline auto const DIR_CONF = DIR / "conf";

inline const uint64_t TEST_TX_GAS_LIMIT = 0;

inline bool wait(nanoseconds timeout, function<bool()> const& condition,
                 nanoseconds poll_period = 1s) {
  assert(poll_period <= timeout);
  for (auto i(timeout / poll_period); i != 0; --i) {
    if (condition()) {
      return true;
    }
    this_thread::sleep_for(poll_period);
  }
  return false;
}

#define ASSERT_HAPPENS(...) ASSERT_TRUE(wait(__VA_ARGS__))
#define EXPECT_HAPPENS(...) EXPECT_TRUE(wait(__VA_ARGS__))

inline auto const node_cfgs_original = Lazy([] {
  vector<FullNodeConfig> ret;
  for (int i = 1;; ++i) {
    auto p = DIR_CONF / (string("conf_taraxa") + std::to_string(i) + ".json");
    if (!fs::exists(p)) {
      break;
    }
    auto& cfg = ret.emplace_back(p.string());
    auto& state_cfg = cfg.chain.final_chain.state;
    auto& dpos_cfg = *state_cfg.dpos;
    state_cfg.genesis_balances = {
        {
            ChainConfig::default_chain_boot_node_addr,
            ChainConfig::default_chain_boot_node_initial_balance +
                dpos_cfg.eligibility_balance_threshold,
        },
    };
    dpos_cfg.genesis_state = {
        {
            ChainConfig::default_chain_boot_node_addr,
            {
                {
                    ChainConfig::default_chain_boot_node_addr,
                    dpos_cfg.eligibility_balance_threshold,
                },
            },
        },
    };
  }
  return ret;
});

template <uint tests_speed = 1, bool enable_rpc_http = false,
          bool enable_rpc_ws = false>
inline auto make_node_cfgs(uint count) {
  static auto const ret = [] {
    auto ret = *node_cfgs_original;
    if constexpr (tests_speed == 1 && enable_rpc_http && enable_rpc_ws) {
      return ret;
    }
    for (auto& cfg : ret) {
      if constexpr (tests_speed != 1) {
        cfg.test_params.block_proposer.min_proposal_delay /= tests_speed;
        cfg.test_params.block_proposer.difficulty_bound = 5;
        cfg.test_params.block_proposer.lambda_bound = 100;
        cfg.chain.pbft.lambda_ms_min /= tests_speed;
      }
      if constexpr (!enable_rpc_http) {
        cfg.rpc.port = nullopt;
      }
      if constexpr (!enable_rpc_ws) {
        cfg.rpc.ws_port = nullopt;
      }
    }
    return ret;
  }();
  if (count == ret.size()) {
    return ret;
  }
  return slice(ret, 0, count);
}

inline auto await_connect(vector<FullNode::Handle> const& nodes) {
  return wait(
      15s,
      [&] {
        for (auto const& node : nodes) {
          if (node->getNetwork()->getPeerCount() < nodes.size() - 1) {
            return false;
          }
        }
        return true;
      },
      500ms);
}

inline auto launch_nodes(vector<FullNodeConfig> const& cfgs,
                         optional<uint> min_peers_to_connect = {},
                         optional<uint> retry_cnt = {}) {
  auto node_count = cfgs.size();
  auto min_peers = min_peers_to_connect.value_or(node_count - 1);
  vector<FullNode::Handle> result(node_count);
  for (auto i = retry_cnt.value_or(3); i != 0; --i, result.clear()) {
    for (uint j = 0; j < node_count; ++j) {
      result[j] = FullNode::Handle(cfgs[j], true);
    }
    if (node_count == 1) {
      return result;
    }
    auto all_connected = wait(
        10s,
        [&] {
          for (auto const& node : result) {
            if (node->getNetwork()->getPeerCount() < min_peers) {
              return false;
            }
          }
          return true;
        },
        500ms);
    if (all_connected) {
      cout << "nodes connected" << endl;
      return result;
    }
  }
  EXPECT_TRUE(false);
  return result;
}

struct BaseTest : virtual WithTestDataDir {
  BaseTest() : WithTestDataDir() {
    for (auto& cfg : *node_cfgs_original) {
      remove_all(cfg.db_path);
    }
  }
  virtual ~BaseTest(){};
};

//#define CORE_TEST(suite_name, test_name, ...)                  \
//  struct suite_name##_##test_name : BaseTest<__VA_ARGS__> {};  \
//  GTEST_TEST_(suite_name, test_name, suite_name##_##test_name, \
//              ::testing::internal::GetTypeId<suite_name##_##test_name>())

inline auto addr(Secret const& secret = Secret::random()) {
  return KeyPair(secret).address();
}

inline auto addr(string const& secret_str) { return addr(Secret(secret_str)); }

struct TransactionClient {
  enum class TransactionStage {
    created,
    inserted,
    executed,
  };
  struct Context {
    TransactionStage stage;
    Transaction trx;
  };

 private:
  shared_ptr<FullNode> node_;
  nanoseconds wait_timeout;
  nanoseconds wait_poll_period;

 public:
  explicit TransactionClient(decltype(node_) node,
                             nanoseconds wait_timeout = 45s,
                             nanoseconds wait_poll_period = 1s)
      : node_(move(node)),
        wait_timeout(wait_timeout),
        wait_poll_period(wait_poll_period) {}

  Context coinTransfer(addr_t const& to, val_t const& val,
                       optional<KeyPair> const& from_k = {},
                       bool wait_executed = true) const {
    // As long as nonce rules are completely disabled, this hack allows to
    // generate unique nonces that contribute to transaction uniqueness.
    // Without this, it's very possible in these tests to have hash collisions,
    // if you just use a constant value like 0 or even get the nonce from the
    // account state. The latter won't work in general because in some tests
    // we don't wait for previous transactions for a sender to complete before
    // sending a new one
    static atomic<uint64_t> nonce = 100000;
    Context ctx{
        TransactionStage::created,
        Transaction(++nonce, val, 0, TEST_TX_GAS_LIMIT, bytes(),
                    from_k ? from_k->secret() : node_->getSecretKey(), to),
    };
    if (!node_->getTransactionManager()
             ->insertTransaction(ctx.trx, false)
             .first) {
      return ctx;
    }
    ctx.stage = TransactionStage::inserted;
    auto trx_hash = ctx.trx.getHash();
    if (wait_executed) {
      auto success = wait(
          wait_timeout,
          [&, this] {
            return node_->getFinalChain()->isKnownTransaction(trx_hash);
          },
          wait_poll_period);
      if (success) {
        ctx.stage = TransactionStage::executed;
      }
    }
    return ctx;
  }
};

};  // namespace taraxa::core_tests

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_HPP_
