#pragma once

#include <libdevcore/SHA3.h>
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

#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "final_chain/contract_interface.hpp"
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

inline const uint64_t TEST_TX_GAS_LIMIT = 1000000;
const auto kContractAddress = addr_t("0x00000000000000000000000000000000000000FE");

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
      vrf_wrapper::vrf_pk_t root_node_vrf_key("d05dc12c1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
      cfg.chain.final_chain.state.genesis_balances[root_node_addr] = 9007199254740991;
      cfg.chain.final_chain.state.genesis_balances[addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0")] =
          9007199254740991;
      cfg.chain.final_chain.state.genesis_balances[addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e")] =
          9007199254740991;
      cfg.chain.final_chain.state.genesis_balances[addr_t("415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0")] =
          9007199254740991;
      cfg.chain.final_chain.state.genesis_balances[addr_t("b770f7a99d0b7ad9adf6520be77ca20ee99b0858")] =
          9007199254740991;
      auto& dpos = *cfg.chain.final_chain.state.dpos;

      state_api::BalanceMap delegations;
      delegations.emplace(root_node_addr, dpos.eligibility_balance_threshold);

      auto initial_validator =
          state_api::ValidatorInfo{root_node_addr, root_node_addr, root_node_vrf_key, 100, "", "", delegations};
      dpos.initial_validators.emplace_back(initial_validator);

      // As test are badly written let's disable it for now
      cfg.chain.final_chain.state.execution_options.disable_nonce_check = true;
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

inline auto make_dpos_trx(const FullNodeConfig& sender_node_cfg, const u256& value = 0, uint64_t nonce = 0,
                          const u256& gas_price = 0) {
  const auto addr = dev::toAddress(sender_node_cfg.node_secret);
  auto proof = dev::sign(sender_node_cfg.node_secret, dev::sha3(addr)).asBytes();
  // We need this for eth compatibility
  proof[64] += 27;
  const auto input = final_chain::ContractInterface::packFunctionCall(
      "registerValidator(address,bytes,bytes,uint16,string,string)", addr, proof,
      vrf_wrapper::getVrfPublicKey(sender_node_cfg.vrf_secret).asBytes(), 10, dev::asBytes("test"),
      dev::asBytes("test"));
  return std::make_shared<Transaction>(nonce, value, gas_price, TEST_TX_GAS_LIMIT, std::move(input),
                                       sender_node_cfg.node_secret, kContractAddress, sender_node_cfg.network.chain_id);
}

inline auto own_balance(std::shared_ptr<FullNode> const& node) {
  return node->getFinalChain()->getBalance(node->getAddress()).first;
}

inline state_api::BalanceMap effective_genesis_balances(const state_api::Config& cfg) {
  if (!cfg.dpos) {
    return cfg.genesis_balances;
  }

  state_api::BalanceMap effective_balances = cfg.genesis_balances;
  for (const auto& validator_info : cfg.dpos->initial_validators) {
    for (const auto& [delegator, amount] : validator_info.delegations) {
      effective_balances[delegator] -= amount;
    }
  }
  return effective_balances;
}

inline auto own_effective_genesis_bal(const FullNodeConfig& cfg) {
  return effective_genesis_balances(cfg.chain.final_chain.state)[dev::toAddress(dev::Secret(cfg.node_secret))];
}

inline auto make_simple_pbft_block(h256 const& hash, uint64_t period, h256 const& anchor_hash = blk_hash_t(0)) {
  std::vector<vote_hash_t> reward_votes_hashes;
  return std::make_shared<PbftBlock>(hash, anchor_hash, blk_hash_t(), period, addr_t(0), secret_t::random(),
                                     std::move(reward_votes_hashes));
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
