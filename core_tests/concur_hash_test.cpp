/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-02-07 15:25:43
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-12 14:08:36
 */

#include "concur_hash.hpp"
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <thread>
#include <vector>
#include "conflict_detector.hpp"

namespace taraxa {
namespace storage {
TEST(ConcurHash, hash_func) {
  unsigned concurrent_degree_exp = 5;
  unsigned num_threads = 8;
  unsigned num_tasks_per_thread = 1234;
  ConflictHash conflict_hash(concurrent_degree_exp);
  conflict_hash.setVerbose(true);
  std::vector<ConflictKey> keys;
  std::vector<ConflictValue> values;
  // generate fake items address
  std::string contract, storage, trx;
  auto init_bucket_size = conflict_hash.getBucketSize();
  for (auto i = 0; i < num_threads * num_tasks_per_thread; ++i) {
    contract += 'a';
    storage += 'b';
    trx += 'c';
    ConflictKey key(contract, storage);
    ConflictValue value(trx, ConflictStatus::read);
    keys.push_back(key);
    values.push_back(value);
  }

  // write to hash_set
  {
    // write using insert
    std::vector<boost::thread> threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      threads.push_back(
          boost::thread([thread_id, num_threads, num_tasks_per_thread, &keys,
                         &values, &conflict_hash]() {
            for (auto i = 0; i < num_tasks_per_thread; ++i) {
              conflict_hash.insert(keys[i * num_threads + thread_id],
                                   values[i * num_threads + thread_id]);
            }
          }));
    }

    std::vector<boost::thread> other_threads;

    // other threads trying to insert same stuff
    // use swapAndInsert this time.
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      other_threads.push_back(boost::thread([thread_id, num_threads,
                                             num_tasks_per_thread, &keys,
                                             &values, &conflict_hash]() {
        for (auto i = 0; i < num_tasks_per_thread; ++i) {
          ConflictValue dummy;
          bool success;
          conflict_hash.compareAndSwap(keys[i * num_threads + thread_id], dummy,
                                       values[i * num_threads + thread_id]);
        }
      }));
    }

    for (auto& t : threads) {
      t.join();
    }

    for (auto& t : other_threads) {
      t.join();
    }
  }

  // read from hash_set, Status == read
  {
    std::vector<std::thread> threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      threads.push_back(
          std::thread([thread_id, num_threads, num_tasks_per_thread, &keys,
                       &conflict_hash] {
            ConflictValue value;
            bool success;
            for (auto i = 0; i < num_tasks_per_thread; ++i) {
              std::tie(value, success) =
                  conflict_hash.get(keys[i * num_threads + thread_id]);
              EXPECT_EQ(success, true);
              EXPECT_EQ(value.getStatus(), ConflictStatus::read);
            }
          }));
    }

    // other threads trying to read same stuff
    std::vector<std::thread> other_threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      other_threads.push_back(
          std::thread([thread_id, num_threads, num_tasks_per_thread, &keys,
                       &conflict_hash] {
            ConflictValue value;
            bool success;
            for (auto i = 0; i < num_tasks_per_thread; ++i) {
              std::tie(value, success) =
                  conflict_hash.get(keys[i * num_threads + thread_id]);
              EXPECT_EQ(success, true);
              EXPECT_EQ(value.getStatus(), ConflictStatus::read);
            }
          }));
    }

    for (auto& t : threads) {
      t.join();
    }

    for (auto& t : other_threads) {
      t.join();
    }
    EXPECT_EQ(conflict_hash.getItemSize(), num_threads * num_tasks_per_thread);
    EXPECT_EQ(conflict_hash.getBucketSize(), 4096);
    EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1 << concurrent_degree_exp);
  }

  // modify hash_set
  {
    std::vector<std::thread> threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      threads.push_back(std::thread([thread_id, num_threads,
                                     num_tasks_per_thread, &keys, &values,
                                     &conflict_hash] {
        ConflictValue value;
        bool success;
        for (auto i = 0; i < num_tasks_per_thread; ++i) {
          ConflictValue expected_value(values[i * num_threads + thread_id]);
          ConflictValue new_value(values[i * num_threads + thread_id].getTrx(),
                                  ConflictStatus::shared);
          success = conflict_hash.compareAndSwap(
              keys[i * num_threads + thread_id], expected_value, new_value);
        }
      }));
    }

    // other threads trying to write
    std::vector<std::thread> other_threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      other_threads.push_back(std::thread([thread_id, num_threads,
                                           num_tasks_per_thread, &keys, &values,
                                           &conflict_hash] {
        ConflictValue value;
        bool success;
        for (auto i = 0; i < num_tasks_per_thread; ++i) {
          ConflictValue expected_value(values[i * num_threads + thread_id]);
          ConflictValue new_value(values[i * num_threads + thread_id].getTrx(),
                                  ConflictStatus::shared);
          success = conflict_hash.compareAndSwap(
              keys[i * num_threads + thread_id], expected_value, new_value);
        }
      }));
    }

    // more threads trying to write
    std::vector<std::thread> alient_threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      alient_threads.push_back(std::thread([thread_id, num_threads,
                                            num_tasks_per_thread, &keys,
                                            &values, &conflict_hash] {
        ConflictValue value;
        bool success;
        for (auto i = 0; i < num_tasks_per_thread; ++i) {
          ConflictValue expected_value(values[i * num_threads + thread_id]);
          ConflictValue new_value(values[i * num_threads + thread_id].getTrx(),
                                  ConflictStatus::shared);
          success = conflict_hash.compareAndSwap(
              keys[i * num_threads + thread_id], expected_value, new_value);
        }
      }));
    }

    for (auto& t : threads) {
      t.join();
    }

    for (auto& t : other_threads) {
      t.join();
    }

    for (auto& t : alient_threads) {
      t.join();
    }

    EXPECT_EQ(conflict_hash.getItemSize(), num_threads * num_tasks_per_thread);
    EXPECT_EQ(conflict_hash.getBucketSize(), 4096);
    EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1 << concurrent_degree_exp);
  }

  // read from hash_set, stauts = shared
  {
    std::vector<std::thread> threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      threads.push_back(
          std::thread([thread_id, num_threads, num_tasks_per_thread, &keys,
                       &conflict_hash] {
            ConflictValue value;
            bool success;
            for (auto i = 0; i < num_tasks_per_thread; ++i) {
              std::tie(value, success) =
                  conflict_hash.get(keys[i * num_threads + thread_id]);
              EXPECT_EQ(success, true);
              EXPECT_EQ(value.getStatus(), ConflictStatus::shared);
            }
          }));
    }

    // other threads trying to read same stuff
    std::vector<std::thread> other_threads;
    for (auto thread_id = 0; thread_id < num_threads; ++thread_id) {
      other_threads.push_back(
          std::thread([thread_id, num_threads, num_tasks_per_thread, &keys,
                       &conflict_hash] {
            ConflictValue value;
            bool success;
            for (auto i = 0; i < num_tasks_per_thread; ++i) {
              std::tie(value, success) =
                  conflict_hash.get(keys[i * num_threads + thread_id]);
              EXPECT_EQ(success, true);
              EXPECT_EQ(value.getStatus(), ConflictStatus::shared);
            }
          }));
    }

    for (auto& t : threads) {
      t.join();
    }

    for (auto& t : other_threads) {
      t.join();
    }
    EXPECT_EQ(conflict_hash.getItemSize(), num_threads * num_tasks_per_thread);
    EXPECT_EQ(conflict_hash.getBucketSize(), 4096);
    EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1 << concurrent_degree_exp);
  }

  // remove half of hash_set
  {
    std::vector<std::thread> threads;
    for (auto thread_id = 0; thread_id < num_threads / 2; ++thread_id) {
      threads.push_back(std::thread([thread_id, num_threads,
                                     num_tasks_per_thread, &keys,
                                     &conflict_hash] {
        for (auto i = 0; i < num_tasks_per_thread; ++i) {
          bool ret = conflict_hash.remove(keys[i * num_threads + thread_id]);
          EXPECT_EQ(ret, true);
        }
      }));
    }
    for (auto& t : threads) {
      t.join();
    }
    EXPECT_EQ(conflict_hash.getItemSize(),
              num_threads * num_tasks_per_thread / 2);
    EXPECT_EQ(conflict_hash.getBucketSize(), 4096);
    EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1 << concurrent_degree_exp);
  }

  // clear hash_set, size reduced to initial
  {
    conflict_hash.clear();
    EXPECT_EQ(conflict_hash.getItemSize(), 0);
    EXPECT_EQ(conflict_hash.getBucketSize(), init_bucket_size);
    EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1 << concurrent_degree_exp);
  }
}

TEST(Detector, conflict_detector_single_thread) {
  struct transaction {
    transaction(trx_t trx, addr_t contract, addr_t storage)
        : trx(trx), contract(contract), storage(storage) {}
    trx_t trx;
    addr_t contract;
    addr_t storage;
  };

  Detector detector(4);

  transaction trans1("trx01", "contract01", "0000");
  transaction trans2("trx02", "contract01", "0000");
  transaction trans3("trx03", "contract01", "0000");

  bool success = false;
  success = detector.load(trans1.contract, trans1.storage, trans1.trx);
  EXPECT_EQ(success, true);  // READ
  success = detector.load(trans2.contract, trans2.storage, trans2.trx);
  EXPECT_EQ(success, true);  // SHARED
  success = detector.load(trans2.contract, trans2.storage, trans2.trx);
  EXPECT_EQ(success, true);  // SHARED
  success = detector.load(trans3.contract, trans3.storage, trans3.trx);
  EXPECT_EQ(success, true);  // SHARED
  success = detector.store(trans1.contract, trans1.storage, trans1.trx);
  EXPECT_EQ(success, false);  // can not write, reject
  success = detector.load(trans1.contract, trans1.storage, trans1.trx);
  EXPECT_EQ(success, true);  // SHARED
  success = detector.load(trans2.contract, trans2.storage, trans2.trx);
  EXPECT_EQ(success, true);  // SHARED
  success = detector.load(trans3.contract, trans3.storage, trans3.trx);
  EXPECT_EQ(success, true);  // SHARED

  transaction trans4("trx04", "contract02", "0000");
  transaction trans5("trx05", "contract02", "0000");
  success = detector.store(trans4.contract, trans4.storage, trans4.trx);
  EXPECT_EQ(success, true);  // WRITE
  success = detector.store(trans5.contract, trans5.storage, trans5.trx);
  EXPECT_EQ(success,
            false);  // differnt transaction write to same entry, reject
  success = detector.store(trans4.contract, trans4.storage, trans4.trx);
  EXPECT_EQ(success, true);  // WRITE
  success = detector.load(trans4.contract, trans4.storage, trans4.trx);
  EXPECT_EQ(success, true);  // WRITE
  success = detector.load(trans5.contract, trans5.storage, trans5.trx);
  EXPECT_EQ(success, false);  // data outdated, reject
  success = detector.load(trans4.contract, trans4.storage, trans4.trx);
  EXPECT_EQ(success, true);  // WRITE
}

}  // namespace storage
}  // namespace taraxa

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
