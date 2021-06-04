#include "final_chain/replay_protection_service.hpp"

#include <filesystem>
#include <optional>
#include <vector>

#include "common/types.hpp"
#include "util_test/gtest.hpp"

namespace taraxa::final_chain {
using namespace std;

struct ReplayProtectionServiceTest : WithDataDir {
  uint64_t range = 0;
  shared_ptr<DbStorage> db{new DbStorage(data_dir)};
  shared_ptr<ReplayProtectionService> SUT;
  vector<vector<ReplayProtectionService::TransactionInfo>> history;
  uint64_t curr_round = 0;

  void init(decltype(range) range, decltype(history) const& history) {
    this->range = range;
    this->history = history;
    SUT = NewReplayProtectionService({range}, db);
  }

  void apply_history(optional<uint64_t> record_count = nullopt) {
    auto cnt = record_count ? *record_count : history.size() - curr_round;
    for (uint64_t i(0); i < cnt; ++i) {
      auto batch = db->createWriteBatch();
      SUT->update(batch, curr_round, history[curr_round]);
      db->commitWriteBatch(batch);
      ++curr_round;
    }
  }

  bool hasNoNonceWatermark(addr_t const& sender_addr) { return !SUT->is_nonce_stale(sender_addr, 0); }

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

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
