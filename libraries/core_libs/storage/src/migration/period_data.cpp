#include "storage/migration/period_data.hpp"

#include <cstdint>

#include "pbft/pbft_manager.hpp"

namespace taraxa::storage::migration {

PeriodData::PeriodData(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string PeriodData::id() { return "PeriodData"; }

uint32_t PeriodData::dbVersion() { return 1; }

void PeriodData::migrate(logger::Logger& log) {
  auto it = db_->getColumnIterator(DB::Columns::period_data);
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

  const auto diff = end_period - start_period;
  uint64_t curr_progress = 0;

  // Get and save data in new format for all blocks
  for (uint64_t i = start_period; i <= end_period; ++i) {
    executor.post([this, i]() {
      const auto bytes = db_->getPeriodDataRaw(i);
      const auto period_data_old_rlp = dev::RLP(bytes);
      assert(period_data_old_rlp.itemCount() == 4);

      ::taraxa::PeriodData period_data;
      period_data.pbft_blk = std::make_shared<PbftBlock>(period_data_old_rlp[0]);
      for (auto const vote_rlp : period_data_old_rlp[1]) {
        period_data.previous_block_cert_votes.emplace_back(std::make_shared<Vote>(vote_rlp));
      }

      for (auto const dag_block_rlp : period_data_old_rlp[2]) {
        period_data.dag_blocks.emplace_back(dag_block_rlp);
      }

      for (auto const trx_rlp : period_data_old_rlp[3]) {
        period_data.transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
      }

      // Reorder transactions
      PbftManager::reorderTransactions(period_data.transactions);
      db_->insert(DB::Columns::period_data, i, period_data.rlp());
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
  // I know it's not perfect to check with sleep, but it's just migration that should be run once
  do {
    taraxa::thisThreadSleepForMilliSeconds(100);
  } while (executor.num_pending_tasks());
}
}  // namespace taraxa::storage::migration