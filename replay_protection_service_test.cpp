#include "replay_protection_service.hpp"

#include <boost/filesystem.hpp>
#include <optional>
#include <vector>

#include "core_tests/util.hpp"
#include "transaction.hpp"
#include "types.hpp"
#include "util/gtest.hpp"

namespace taraxa::replay_protection_service {
using boost::filesystem::create_directories;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using std::cout;
using std::list;
using std::make_shared;
using std::nullopt;
using std::optional;
using std::shared_ptr;
using std::vector;
using taraxa::s_ptr;

// TODO more tests
struct ReplayProtectionServiceTest : testing::Test {
  round_t range = 0;
  shared_ptr<DbStorage> db;
  shared_ptr<ReplayProtectionService> SUT;
  vector<vector<ReplayProtectionService::TransactionInfo>> history;
  round_t curr_round = 0;

  void init(decltype(range) range, decltype(history) const& history) {
    static auto const DB_DIR =
        temp_directory_path() / "taraxa" / "replay_protection_service_test";
    remove_all(DB_DIR);
    this->range = range;
    this->history = history;
    db = DbStorage::make(DB_DIR, h256::random(), true);
    SUT = s_ptr(new ReplayProtectionService(range, db));
  }

  void apply_history(optional<round_t> record_count = nullopt) {
    auto cnt = record_count ? *record_count : history.size() - curr_round;
    for (round_t i(0); i < cnt; ++i) {
      auto batch = db->createWriteBatch();
      SUT->register_executed_transactions(batch, curr_round,
                                          history[curr_round]);
      db->commitWriteBatch(batch);
      ++curr_round;
    }
  }

  bool hasNoNonceWatermark(addr_t const& sender_addr) {
    return !SUT->is_nonce_stale(sender_addr, 0);
  }

  bool hasNonceWatermark(addr_t const& sender_addr, uint64_t watermark) {
    for (uint64_t i(0); i <= watermark; ++i) {
      if (!SUT->is_nonce_stale(sender_addr, i)) {
        return false;
      }
    }
    return !SUT->is_nonce_stale(sender_addr, watermark + 1);
  }
};

TEST_F(ReplayProtectionServiceTest, single_sender) {
  auto sender_1 = addr_t::random();
  init(3, {
              {},
              {},
              {},
              {},
              {
                  {sender_1, 0},
                  {sender_1, 3},
              },
              {
                  {sender_1, 5},
                  {sender_1, 1},
              },
              {
                  {sender_1, 5},
                  {sender_1, 1},
              },
              {},
          });
  apply_history();
  EXPECT_TRUE(hasNonceWatermark(sender_1, 3));
}

TEST_F(ReplayProtectionServiceTest, multi_senders_1) {
  auto sender_1 = addr_t::random();
  auto sender_2 = addr_t::random();
  auto sender_3 = addr_t::random();
  init(2, {
              {
                  {sender_1, 0},
                  {sender_2, 3},
              },
              {
                  {sender_1, 5},
                  {sender_2, 1},
              },
              {
                  {sender_1, 5},
                  {sender_1, 1},
              },
              {
                  {sender_3, 2},
                  {sender_3, 0},
              },
              {
                  {sender_3, 1},
              },
          });
  apply_history(3);
  EXPECT_TRUE(hasNonceWatermark(sender_1, 0));
  EXPECT_TRUE(hasNonceWatermark(sender_2, 3));
  apply_history();
  EXPECT_TRUE(hasNonceWatermark(sender_1, 5));
  EXPECT_TRUE(hasNonceWatermark(sender_2, 3));
  EXPECT_TRUE(hasNoNonceWatermark(sender_3));
}

TEST_F(ReplayProtectionServiceTest, multi_senders_2) {
  auto sender_1 = addr_t::random();
  auto sender_2 = addr_t::random();
  init(1, {
              {
                  {sender_1, 5},
                  {sender_1, 1},
                  {sender_2, 6},
                  {sender_1, 3},
              },
              {},
              {},
              {},
          });
  apply_history(1);
  while (curr_round < history.size()) {
    apply_history(1);
    EXPECT_TRUE(hasNonceWatermark(sender_1, 5));
    EXPECT_TRUE(hasNonceWatermark(sender_2, 6));
  }
}

}  // namespace taraxa::replay_protection_service

TARAXA_TEST_MAIN({})
