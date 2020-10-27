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

struct wait_opts {
  nanoseconds timeout;
  nanoseconds poll_period = 1s;
};
class wait_ctx {
  bool failed_ = false;

 public:
  bool const is_last_attempt;
  uint64_t const attempt;
  uint64_t const attempt_count;
  ostream& fail_log;

  wait_ctx(uint64_t attempt, uint64_t attempt_count, ostream& fail_log)
      : is_last_attempt(attempt == attempt_count - 1),
        attempt(attempt),
        attempt_count(attempt_count),
        fail_log(fail_log) {}

  void fail() { failed_ = true; }
  auto fail_if(bool cond) {
    if (cond) {
      failed_ = true;
    }
    return failed_;
  }
  auto failed() const { return failed_; }
};
inline bool wait(wait_opts const& opts,
                 function<void(wait_ctx&)> const& poller) {
  struct NullBuffer : streambuf {
    int overflow(int c) override { return c; }
  } static null_buf;

  assert(opts.poll_period <= opts.timeout);
  uint64_t attempt_count = opts.timeout / opts.poll_period;
  for (uint64_t i(0); i < attempt_count; ++i) {
    if (i == attempt_count - 1) {
      stringstream err_log;
      wait_ctx ctx(i, attempt_count, err_log);
      if (poller(ctx); !ctx.failed()) {
        return true;
      }
      if (auto const& s = err_log.str(); !s.empty()) {
        cout << err_log.str() << endl;
      }
      return false;
    }
    ostream null_strm(&null_buf);
    wait_ctx ctx(i, attempt_count, null_strm);
    if (poller(ctx); !ctx.failed()) {
      return true;
    }
    this_thread::sleep_for(opts.poll_period);
  }
  assert(false);
}
#define ASSERT_HAPPENS(...) ASSERT_TRUE(wait(__VA_ARGS__))
#define EXPECT_HAPPENS(...) EXPECT_TRUE(wait(__VA_ARGS__))
#define WAIT_EXPECT_EQ(ctx, o1, o2)         \
  if (o1 != o2) {                           \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_EQ(o1, o2);                      \
  }                                         \
  assert(true)  // to justify trailing semicolon

inline auto const node_cfgs_original = Lazy([] {
  vector<FullNodeConfig> ret;
  for (int i = 1;; ++i) {
    auto p = DIR_CONF / (string("conf_taraxa") + std::to_string(i) + ".json");
    if (!fs::exists(p)) {
      break;
    }
    ret.emplace_back(p.string());
  }
  return ret;
});

template <uint tests_speed = 1, bool enable_rpc_http = false,
          bool enable_rpc_ws = false>
inline auto make_node_cfgs(uint count) {
  static_assert(tests_speed <= 5);
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
        cfg.network.network_transaction_interval /= tests_speed;
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

inline auto wait_connect(vector<FullNode::Handle> const& nodes,
                         optional<uint> min_peers_to_connect = {}) {
  auto min_peers = min_peers_to_connect.value_or(nodes.size() - 1);
  return wait({30s, 1s}, [&](auto& ctx) {
    for (auto const& node : nodes) {
      if (ctx.fail_if(node->getNetwork()->getPeerCount() < min_peers)) {
        return;
      }
    }
  });
}

inline auto launch_nodes(vector<FullNodeConfig> const& cfgs,
                         optional<uint> min_peers_to_connect = {},
                         optional<uint> retry_cnt = {}) {
  auto node_count = cfgs.size();
  for (auto i = retry_cnt.value_or(4);; --i) {
    vector<FullNode::Handle> nodes(node_count);
    for (uint j = 0; j < node_count; ++j) {
      if (j > 0) {
        this_thread::sleep_for(500ms);
      }
      nodes[j] = FullNode::Handle(cfgs[j], true);
    }
    if (node_count == 1) {
      return nodes;
    }
    if (wait_connect(nodes, min_peers_to_connect)) {
      cout << "nodes connected" << endl;
      return nodes;
    }
    if (i == 0) {
      EXPECT_TRUE(false) << "nodes didn't connect properly";
      return nodes;
    }
  }
}

struct BaseTest : virtual WithDataDir {
  BaseTest() : WithDataDir() {
    for (auto& cfg : *node_cfgs_original) {
      remove_all(cfg.db_path);
      create_directories(cfg.db_path);
    }
  }
  virtual ~BaseTest(){};
};

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
  wait_opts wait_opts_;

 public:
  explicit TransactionClient(decltype(node_) node,
                             wait_opts const& wait_opts = {60s, 1s})
      : node_(move(node)), wait_opts_(wait_opts) {}

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
      auto success = wait(wait_opts_, [&, this](auto& ctx) {
        ctx.fail_if(!node_->getFinalChain()->isKnownTransaction(trx_hash));
      });
      if (success) {
        ctx.stage = TransactionStage::executed;
      }
    }
    return ctx;
  }
};

inline auto make_dpos_trx(FullNodeConfig const& sender_node_cfg,
                          state_api::DPOSTransfers const& transfers,
                          uint64_t nonce = 0, u256 const& gas_price = 0,
                          uint64_t extra_gas = 0) {
  StateAPI::DPOSTransactionPrototype proto(transfers);
  return Transaction(nonce, proto.value, gas_price,
                     proto.minimal_gas + extra_gas, std::move(proto.input),
                     dev::Secret(sender_node_cfg.node_secret), proto.to,
                     sender_node_cfg.chain.chain_id);
}

inline auto own_balance(shared_ptr<FullNode> const& node) {
  return node->getFinalChain()->getBalance(node->getAddress()).first;
}

inline auto own_effective_genesis_bal(FullNodeConfig const& cfg) {
  return cfg.chain.final_chain.state.effective_genesis_balance(
      dev::toAddress(dev::Secret(cfg.node_secret)));
}

inline auto make_simple_pbft_block(h256 const& hash, uint64_t period) {
  return PbftBlock(hash, blk_hash_t(0), TrxSchedule(), period, addr_t(0),
                   secret_t::random());
}

};  // namespace taraxa::core_tests

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_HPP_
