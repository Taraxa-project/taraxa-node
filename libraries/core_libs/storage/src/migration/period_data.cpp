#include "storage/migration/period_data.hpp"

#include <cstdint>

#include "pbft/pbft_manager.hpp"

namespace taraxa::storage::migration {

PeriodData::PeriodData(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string PeriodData::id() { return "PeriodData"; }

uint32_t PeriodData::dbVersion() { return 1; }

void PeriodData::migrate(logger::Logger& log) {
  auto orig_col = DB::Columns::period_data;
  auto copied_col = db_->copyColumn(db_->handle(orig_col), orig_col.name() + "-copy");

  if (copied_col == nullptr) {
    LOG(log) << "Migration " << id() << " skipped: Unable to copy " << orig_col.name() << " column";
    return;
  }

  auto it = db_->getColumnIterator(copied_col.get());
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
  for (uint64_t i = start_period; i <= end_period; ++i) {
    executor.post([this, i, &copied_col]() {
      const auto bytes = db_->getPeriodDataRaw(i);
      const auto period_data_old_rlp = dev::RLP(bytes);
      assert(period_data_old_rlp.itemCount() == 4);

      auto period_data = ::taraxa::PeriodData(period_data_old_rlp);

      // Reorder transactions
      PbftManager::reorderTransactions(period_data.transactions);
      db_->insert(copied_col.get(), i, period_data.rlp());
    });
    // This should slow down main loop so we are not using so much memory
    while (executor.num_pending_tasks() > (executor.capacity() * 3)) {
      taraxa::thisThreadSleepForMilliSeconds(50);
    }
    auto percentage = (i - start_period) * 100 / diff;
    if (percentage > curr_progress) {
      curr_progress = percentage;
      LOG(log) << "Migration " << id() << " progress " << curr_progress << "%";
    }
  }

  // It's not perfect to check with sleep, but it's just migration that should be run once
  do {
    taraxa::thisThreadSleepForMilliSeconds(100);
  } while (executor.num_pending_tasks());

  db_->replaceColumn(orig_col, std::move(copied_col));
}
}  // namespace taraxa::storage::migration