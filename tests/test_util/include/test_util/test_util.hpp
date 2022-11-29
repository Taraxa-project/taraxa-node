#pragma once

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "../../gtest.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "final_chain/contract_interface.hpp"
#include "network/network.hpp"
#include "node/node.hpp"
#include "transaction/transaction_manager.hpp"

// TODO rename this namespace to `util_test`
namespace taraxa {
using dev::KeyPair;
using dev::Secret;
using std::filesystem::is_regular_file;
using std::filesystem::path;
using std::filesystem::recursive_directory_iterator;
using std::filesystem::remove_all;
using namespace std::chrono;
using expected_balances_map_t = std::map<addr_t, u256>;
using shared_nodes_t = std::vector<std::shared_ptr<FullNode>>;

const uint64_t TEST_TX_GAS_LIMIT = 500000;
const uint64_t TEST_BLOCK_GAS_LIMIT = ((uint64_t)1 << 53) - 1;
const auto kContractAddress = addr_t("0x00000000000000000000000000000000000000FE");

inline auto addr(const Secret& secret = Secret::random()) { return KeyPair(secret).address(); }

inline auto addr(const string& secret_str) { return addr(Secret(secret_str)); }

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
bool wait(const wait_opts& opts, const std::function<void(wait_ctx&)>& poller);
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
  uint64_t nonce_{1};
  Secret secret_;

 public:
  explicit TransactionClient(decltype(node_) node, std::optional<Secret> secret = {},
                             const wait_opts& wait_opts = {60s, 1s})
      : node_(std::move(node)), wait_opts_(wait_opts), secret_(secret.has_value() ? *secret : node_->getSecretKey()) {}

  void must_process_sync(const std::shared_ptr<Transaction>& trx) const;

  Context process(const std::shared_ptr<Transaction>& trx, bool wait_executed = true) const;

  Context coinTransfer(const addr_t& to, const val_t& val, bool wait_executed = true);
};

SharedTransaction make_dpos_trx(const FullNodeConfig& sender_node_cfg, const u256& value = 0, uint64_t nonce = 0,
                                const u256& gas_price = 0);

u256 own_balance(const std::shared_ptr<FullNode>& node);

state_api::BalanceMap effective_initial_balances(const state_api::Config& cfg);

u256 own_effective_genesis_bal(const FullNodeConfig& cfg);

std::shared_ptr<PbftBlock> make_simple_pbft_block(const h256& hash, uint64_t period,
                                                  const h256& anchor_hash = blk_hash_t(0));

std::vector<blk_hash_t> getOrderedDagBlocks(const std::shared_ptr<DbStorage>& db);

addr_t make_addr(uint8_t i);

void wait_for_balances(const std::vector<std::shared_ptr<FullNode>>& nodes, const expected_balances_map_t& balances,
                       wait_opts to_wait = {10s, 500ms});

struct NodesTest : virtual WithDataDir {
  virtual ~NodesTest() {}
  NodesTest();
  NodesTest(const NodesTest&) = delete;
  NodesTest(NodesTest&&) = delete;
  NodesTest& operator=(const NodesTest&) = delete;
  NodesTest& operator=(NodesTest&&) = delete;

  void overwriteFromJsons();

  void CleanupDirs();

  void TearDown() override;

  std::vector<taraxa::FullNodeConfig> make_node_cfgs(size_t total_count, size_t validators_count = 1,
                                                     uint tests_speed = 1, bool enable_rpc_http = false,
                                                     bool enable_rpc_ws = false);

  bool wait_connect(const std::vector<std::shared_ptr<taraxa::FullNode>>& nodes);

  shared_nodes_t create_nodes(uint count, bool start = false);

  shared_nodes_t create_nodes(const std::vector<FullNodeConfig>& cfgs, bool start = false);

  shared_nodes_t launch_nodes(const std::vector<taraxa::FullNodeConfig>& cfgs);

  std::vector<taraxa::FullNodeConfig> node_cfgs;
};

}  // namespace taraxa
