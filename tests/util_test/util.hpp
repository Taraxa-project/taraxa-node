#pragma once

#include <libdevcrypto/Common.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "config/config.hpp"
#include "gtest.hpp"
#include "network/network.hpp"
#include "node/node.hpp"
#include "transaction/transaction_manager.hpp"

// TODO rename this namespace to `util_test`
namespace taraxa::core_tests {
using dev::KeyPair;
using dev::Secret;
using std::filesystem::is_regular_file;
using std::filesystem::path;
using std::filesystem::recursive_directory_iterator;
using std::filesystem::remove_all;
using namespace std::chrono;

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
  std::ostream& fail_log;

  wait_ctx(uint64_t attempt, uint64_t attempt_count, std::ostream& fail_log)
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
inline bool wait(wait_opts const& opts, std::function<void(wait_ctx&)> const& poller) {
  struct NullBuffer : std::streambuf {
    int overflow(int c) override { return c; }
  } static null_buf;

  assert(opts.poll_period <= opts.timeout);
  uint64_t attempt_count = opts.timeout / opts.poll_period;
  for (uint64_t i(0); i < attempt_count; ++i) {
    if (i == attempt_count - 1) {
      std::stringstream err_log;
      wait_ctx ctx(i, attempt_count, err_log);
      if (poller(ctx); !ctx.failed()) {
        return true;
      }
      if (auto const& s = err_log.str(); !s.empty()) {
        std::cout << err_log.str() << std::endl;
      }
      return false;
    }
    std::ostream null_strm(&null_buf);
    wait_ctx ctx(i, attempt_count, null_strm);
    if (poller(ctx); !ctx.failed()) {
      return true;
    }
    std::this_thread::sleep_for(opts.poll_period);
  }
  assert(false);
}
#define ASSERT_HAPPENS(...) ASSERT_TRUE(wait(__VA_ARGS__))
#define EXPECT_HAPPENS(...) EXPECT_TRUE(wait(__VA_ARGS__))

#define WAIT_EXPECT_TRUE(ctx, o)            \
  if (!o) {                                 \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_TRUE(o);                         \
  }

#define WAIT_EXPECT_EQ(ctx, o1, o2)         \
  if (o1 != o2) {                           \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_EQ(o1, o2);                      \
  }

#define WAIT_EXPECT_NE(ctx, o1, o2)         \
  if (o1 == o2) {                           \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_NE(o1, o2);                      \
  }

#define WAIT_EXPECT_LT(ctx, o1, o2)         \
  if (o1 >= o2) {                           \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_LT(o1, o2);                      \
  }

#define WAIT_EXPECT_GT(ctx, o1, o2)         \
  if (o1 <= o2) {                           \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_GT(o1, o2);                      \
  }

#define WAIT_EXPECT_GE(ctx, o1, o2)         \
  if (o1 < o2) {                            \
    if (ctx.fail(); !ctx.is_last_attempt) { \
      return;                               \
    }                                       \
    EXPECT_GE(o1, o2);                      \
  }

template <uint tests_speed = 1, bool enable_rpc_http = false, bool enable_rpc_ws = false>
inline auto make_node_cfgs(uint count) {
  static auto const ret = [] {
    auto ret = *node_cfgs_original;
    if constexpr (tests_speed == 1 && enable_rpc_http && enable_rpc_ws) {
      return ret;
    }
    for (auto& cfg : ret) {
      addr_t root_node_addr("de2b1203d72d3549ee2f733b00b2789414c7cea5");
      cfg.chain.final_chain.state.genesis_balances[root_node_addr] = 9007199254740991;
      auto& dpos = *cfg.chain.final_chain.state.dpos;
      dpos.genesis_state[root_node_addr][root_node_addr] = dpos.eligibility_balance_threshold;
      // As test are badly written let's disable it for now
      cfg.chain.final_chain.state.execution_options.disable_nonce_check = true;
      cfg.chain.final_chain.state.execution_options.disable_gas_fee = true;
      cfg.chain.final_chain.state.block_rewards_options.disable_block_rewards = true;
      cfg.chain.final_chain.state.block_rewards_options.disable_contract_distribution = true;
      if constexpr (tests_speed != 1) {
        // VDF config
        cfg.chain.sortition.vrf.threshold_upper = 0xffff;
        cfg.chain.sortition.vrf.threshold_range = 0x199a;
        cfg.chain.sortition.vdf.difficulty_min = 0;
        cfg.chain.sortition.vdf.difficulty_max = 5;
        cfg.chain.sortition.vdf.difficulty_stale = 5;
        cfg.chain.sortition.vdf.lambda_bound = 100;
        // PBFT config
        cfg.chain.pbft.lambda_ms_min /= tests_speed;
        cfg.network.network_transaction_interval /= tests_speed;
      }
      if constexpr (!enable_rpc_http) {
        cfg.rpc->http_port = std::nullopt;
      }
      if constexpr (!enable_rpc_ws) {
        cfg.rpc->ws_port = std::nullopt;
      }
    }
    return ret;
  }();
  if (count == ret.size()) {
    return ret;
  }
  return slice(ret, 0, count);
}

inline auto wait_connect(std::vector<std::shared_ptr<FullNode>> const& nodes) {
  auto num_peers_connected = nodes.size() - 1;
  return wait({30s, 1s}, [&](auto& ctx) {
    for (auto const& node : nodes) {
      if (ctx.fail_if(node->getNetwork()->getPeerCount() < num_peers_connected)) {
        return;
      }
    }
  });
}

inline auto create_nodes(uint count, bool start = false) {
  auto cfgs = make_node_cfgs(count);
  auto node_count = cfgs.size();
  std::vector<std::shared_ptr<FullNode>> nodes;
  for (uint j = 0; j < node_count; ++j) {
    if (j > 0) {
      std::this_thread::sleep_for(500ms);
    }
    nodes.emplace_back(std::make_shared<FullNode>(cfgs[j]));
    if (start) nodes.back()->start();
  }
  return nodes;
}

inline auto create_nodes(std::vector<FullNodeConfig> const& cfgs, bool start = false) {
  auto node_count = cfgs.size();
  std::vector<std::shared_ptr<FullNode>> nodes;
  for (uint j = 0; j < node_count; ++j) {
    if (j > 0) {
      std::this_thread::sleep_for(500ms);
    }
    nodes.emplace_back(std::make_shared<FullNode>(cfgs[j]));
    if (start) nodes.back()->start();
  }
  return nodes;
}

inline auto launch_nodes(std::vector<FullNodeConfig> const& cfgs) {
  constexpr auto RETRY_COUNT = 4;
  auto node_count = cfgs.size();
  for (auto i = RETRY_COUNT;; --i) {
    auto nodes = create_nodes(cfgs, true);
    if (node_count == 1) {
      return nodes;
    }
    if (wait_connect(nodes)) {
      std::cout << "Nodes connected and initial status packets passed" << std::endl;
      return nodes;
    }
    if (i == 0) {
      EXPECT_TRUE(false) << "nodes didn't connect properly";
      return nodes;
    }
  }
}

inline auto addr(Secret const& secret = Secret::random()) { return KeyPair(secret).address(); }

inline auto addr(string const& secret_str) { return addr(Secret(secret_str)); }

struct TransactionClient {
  enum class TransactionStage {
    created,
    inserted,
    executed,
  };
  struct Context {
    TransactionStage stage;
    std::shared_ptr<Transaction> trx;
  };

 private:
  std::shared_ptr<FullNode> node_;
  wait_opts wait_opts_;

 public:
  explicit TransactionClient(decltype(node_) node, wait_opts const& wait_opts = {60s, 1s})
      : node_(move(node)), wait_opts_(wait_opts) {}

  void must_process_sync(std::shared_ptr<Transaction> const& trx) const {
    ASSERT_EQ(process(trx, true).stage, TransactionStage::executed);
  }

  Context process(std::shared_ptr<Transaction> const& trx, bool wait_executed = true) const {
    Context ctx{
        TransactionStage::created,
        trx,
    };
    if (!node_->getTransactionManager()->insertTransaction(ctx.trx).first) {
      return ctx;
    }
    ctx.stage = TransactionStage::inserted;
    auto trx_hash = ctx.trx->getHash();
    if (wait_executed) {
      auto success = wait(
          wait_opts_, [&, this](auto& ctx) { ctx.fail_if(!node_->getFinalChain()->transaction_location(trx_hash)); });
      if (success) {
        ctx.stage = TransactionStage::executed;
      }
    }
    return ctx;
  }

  Context coinTransfer(addr_t const& to, val_t const& val, std::optional<KeyPair> const& from_k = {},
                       bool wait_executed = true) const {
    // As long as nonce rules are completely disabled, this hack allows to
    // generate unique nonces that contribute to transaction uniqueness.
    // Without this, it's very possible in these tests to have hash collisions,
    // if you just use a constant value like 0 or even get the nonce from the
    // account state. The latter won't work in general because in some tests
    // we don't wait for previous transactions for a sender to complete before
    // sending a new one
    static std::atomic<uint64_t> nonce = 100000;
    return process(std::make_shared<Transaction>(++nonce, val, 0, TEST_TX_GAS_LIMIT, bytes(),
                                                 from_k ? from_k->secret() : node_->getSecretKey(), to),
                   wait_executed);
  }
};

// inline auto make_dpos_trx(FullNodeConfig const& sender_node_cfg, state_api::DPOSTransfers const& transfers,
// uint64_t nonce = 0, u256 const& gas_price = 0, uint64_t extra_gas = 0) {
// StateAPI::DPOSTransactionPrototype proto(transfers);
// return std::make_shared<Transaction>(nonce, proto.value, gas_price, proto.minimal_gas + extra_gas,
//                                      std::move(proto.input), dev::Secret(sender_node_cfg.node_secret), proto.to,
//                                      sender_node_cfg.chain.chain_id);
// }

inline auto own_balance(std::shared_ptr<FullNode> const& node) {
  return node->getFinalChain()->getBalance(node->getAddress()).first;
}

inline auto own_effective_genesis_bal(FullNodeConfig const& cfg) {
  return cfg.chain.final_chain.state.effective_genesis_balance(dev::toAddress(dev::Secret(cfg.node_secret)));
}

inline auto make_simple_pbft_block(h256 const& hash, uint64_t period, h256 const& anchor_hash = blk_hash_t(0)) {
  return PbftBlock(hash, anchor_hash, blk_hash_t(), period, addr_t(0), secret_t::random());
}

inline std::vector<blk_hash_t> getOrderedDagBlocks(std::shared_ptr<DbStorage> const& db) {
  uint64_t period = 1;
  std::vector<blk_hash_t> res;
  while (true) {
    auto pbft_block = db->getPbftBlock(period);
    if (pbft_block.has_value()) {
      for (auto& dag_block_hash : db->getFinalizedDagBlockHashesByPeriod(period)) {
        res.push_back(dag_block_hash);
      }
      period++;
      continue;
    }
    break;
  }
  return res;
}

inline auto make_addr(uint8_t i) {
  addr_t ret;
  ret[10] = i;
  return ret;
}

using expected_balances_map_t = std::map<addr_t, u256>;
inline void wait_for_balances(const std::vector<std::shared_ptr<FullNode>>& nodes,
                              const expected_balances_map_t& balances, wait_opts to_wait = {10s, 500ms}) {
  auto sendDummyTransaction = [&](const auto& trx_client) {
    trx_client.coinTransfer(KeyPair::create().address(), 0, KeyPair::create(), false);
  };
  wait(to_wait, [&](auto& ctx) {
    for (const auto& node : nodes) {
      for (const auto& b : balances) {
        if (node->getFinalChain()->getBalance(b.first).first != b.second) {
          sendDummyTransaction(TransactionClient(node));
          WAIT_EXPECT_EQ(ctx, node->getFinalChain()->getBalance(b.first).first, b.second);
        }
      }
      // wait for the same chain size on all nodes
      for (const auto& n : nodes) {
        WAIT_EXPECT_EQ(ctx, node->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                       n->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks());
      }
    }
  });
}

}  // namespace taraxa::core_tests
