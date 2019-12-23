#ifndef TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_TEST_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_TEST_HPP_

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <optional>
#include <vector>
#include "core_tests/util.hpp"
#include "replay_protection_service.hpp"
#include "transaction.hpp"
#include "types.hpp"
#include "db_storage.hpp"

namespace taraxa::replay_protection::replay_protection_service_test {
using boost::filesystem::create_directories;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using dev::h64;
using replay_protection_service::ReplayProtectionService;
using std::cout;
using std::list;
using std::make_shared;
using std::nullopt;
using std::optional;
using std::shared_ptr;
using std::vector;
using taraxa::as_shared;
using taraxa::Transaction;
using taraxa::DbStorage;

// TODO more tests
struct ReplayProtectionServiceTest : testing::Test {
  round_t range = 0;
  shared_ptr<DbStorage> db;
  shared_ptr<ReplayProtectionService> sut;
  vector<ReplayProtectionService::transaction_batch_t> history;
  round_t curr_round = 0;

  static auto makeTrx(trx_nonce_t nonce,
                      secret_t const& sender_sk = secret_t::random()) {
    return as_shared(new Transaction(nonce,                    // nonce
                                     0,                        // value
                                     0,                        // gas_price
                                     0,                        // gas
                                     addr_t::random(),         // receiver
                                     h64::random().asBytes(),  // data
                                     sender_sk));              // secret
  }

  void init(decltype(range) range, decltype(history) const& history) {
    static auto const DB_DIR =
        temp_directory_path() / "taraxa" / "replay_protection_service_test";
    remove_all(DB_DIR);
    create_directories(DB_DIR);
    this->range = range;
    this->history = history;
    db = make_shared<DbStorage>(DB_DIR, blk_hash_t(), true);
    sut = as_shared(new ReplayProtectionService(range, db));
  }

  void apply_history(optional<round_t> record_count = nullopt) {
    auto cnt = record_count ? *record_count : history.size() - curr_round;
    for (round_t i(0); i < cnt; ++i) {
      sut->commit(curr_round, history[curr_round]);
      ++curr_round;
    }
  }

  void check_history_not_replayable() {
    for (auto& trxs : history) {
      for (auto& t : trxs) {
        EXPECT_TRUE(sut->hasBeenExecuted(*t));
      }
    }
  }

  bool hasNoNonceWatermark(secret_t const& sender_sk) {
    return !sut->hasBeenExecuted(*makeTrx(0, sender_sk));
  }

  bool hasNonceWatermark(secret_t const& sender_sk, trx_nonce_t watermark) {
    for (trx_nonce_t i(0); i <= watermark; ++i) {
      if (!sut->hasBeenExecuted(*makeTrx(i, sender_sk))) {
        return false;
      }
    }
    return !sut->hasBeenExecuted(*makeTrx(watermark + 1, sender_sk));
  }
};

TEST_F(ReplayProtectionServiceTest, single_sender) {
  auto sender_1 = secret_t::random();
  init(3, {
              {},
              {},
              {},
              {},
              {
                  makeTrx(0, sender_1),
                  makeTrx(3, sender_1),
              },
              {
                  makeTrx(5, sender_1),
                  makeTrx(1, sender_1),
              },
              {
                  makeTrx(5, sender_1),
                  makeTrx(1, sender_1),
              },
              {},
          });
  apply_history();
  EXPECT_TRUE(hasNonceWatermark(sender_1, 3));
  check_history_not_replayable();
}

TEST_F(ReplayProtectionServiceTest, multi_senders_1) {
  auto sender_1 = secret_t::random();
  auto sender_2 = secret_t::random();
  auto sender_3 = secret_t::random();
  init(2, {
              {
                  makeTrx(0, sender_1),
                  makeTrx(3, sender_2),
              },
              {
                  makeTrx(5, sender_1),
                  makeTrx(1, sender_2),
              },
              {
                  makeTrx(5, sender_1),
                  makeTrx(1, sender_1),
              },
              {
                  makeTrx(2, sender_3),
                  makeTrx(0, sender_3),
              },
              {
                  makeTrx(1, sender_3),
              },
          });
  apply_history(3);
  EXPECT_TRUE(hasNonceWatermark(sender_1, 0));
  EXPECT_TRUE(hasNonceWatermark(sender_2, 3));
  apply_history();
  EXPECT_TRUE(hasNonceWatermark(sender_1, 5));
  EXPECT_TRUE(hasNonceWatermark(sender_2, 3));
  EXPECT_TRUE(hasNoNonceWatermark(sender_3));
  check_history_not_replayable();
}

TEST_F(ReplayProtectionServiceTest, multi_senders_2) {
  auto sender_1 = secret_t::random();
  auto sender_2 = secret_t::random();
  init(1, {
              {
                  makeTrx(5, sender_1),
                  makeTrx(1, sender_1),
                  makeTrx(6, sender_2),
                  makeTrx(3, sender_1),
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
  check_history_not_replayable();
}

};  // namespace taraxa::replay_protection::replay_protection_service_test

#endif  // TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_TEST_HPP_
