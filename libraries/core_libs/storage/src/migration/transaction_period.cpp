#include "storage/migration/transaction_period.hpp"

#include <cstdint>

#include "common/thread_pool.hpp"
#include "common/util.hpp"

namespace taraxa::storage::migration {

TransactionPeriod::TransactionPeriod(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string TransactionPeriod::id() { return "TransactionPeriod"; }

uint32_t TransactionPeriod::dbVersion() { return 1; }

void TransactionPeriod::migrate(logger::Logger& log) {
  auto it = db_->getColumnIterator(DbStorage::Columns::period_data);
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  uint64_t start_period, end_period;
  memcpy(&start_period, it->key().data(), sizeof(uint64_t));

  it->SeekToLast();
  if (!it->Valid()) {
    it->Prev();
  }
  memcpy(&end_period, it->key().data(), sizeof(uint64_t));
  util::ThreadPool executor{std::thread::hardware_concurrency()};

  const auto diff = (end_period - start_period) ? (end_period - start_period) : 1;
  uint64_t curr_progress = 0;

  // Get and save data in new format for all blocks
  for (uint64_t period = start_period; period <= end_period; ++period) {
    executor.post([this, period]() {
      auto batch = db_->createWriteBatch();
      const auto& transactions = db_->getPeriodTransactions(period);
      for (uint64_t position = 0; position < transactions->size(); ++position) {
        db_->addTransactionLocationToBatch(batch, (*transactions)[position]->getHash(), period, position);
      }
      db_->commitWriteBatch(batch);
    });
    // This should slow down main loop so we are not using too much memory
    while (executor.num_pending_tasks() > (executor.capacity() * 3)) {
      taraxa::thisThreadSleepForMilliSeconds(1);
    }
    auto percentage = (period - start_period) * 100 / diff;
    if (percentage > curr_progress) {
      curr_progress = percentage;
      LOG(log) << "Migration " << id() << " progress " << curr_progress << "%";
    }
  }

  // It's not perfect to check with sleep, but it's just migration that should be run once
  do {
    taraxa::thisThreadSleepForMilliSeconds(100);
  } while (executor.num_pending_tasks());
}
}  // namespace taraxa::storage::migration