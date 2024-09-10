#include "storage/migration/dag_block_period_migration.hpp"

#include <libdevcore/Common.h>

#include <cstdint>

#include "pbft/period_data.hpp"

namespace taraxa::storage::migration {

PeriodDataDagBlockMigration::PeriodDataDagBlockMigration(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string PeriodDataDagBlockMigration::id() { return "PeriodDataDagBlockMigration"; }

uint32_t PeriodDataDagBlockMigration::dbVersion() { return 1; }

void PeriodDataDagBlockMigration::migrate(logger::Logger& log) {
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
  const auto diff = (end_period - start_period) ? (end_period - start_period) : 1;

  uint64_t curr_progress = 0;
  auto batch = db_->createWriteBatch();
  const size_t max_size = 500000000;

  // Get and save data in new format for all blocks
  for (uint64_t period = start_period; period <= end_period; period++) {
    const auto bts = db_->getPeriodDataRaw(period);
    const auto db_rlp = dev::RLP(bts);
    auto percentage = (period - start_period) * 100 / diff;
    if (percentage > curr_progress) {
      curr_progress = percentage;
      LOG(log) << "Migration " << id() << " progress " << curr_progress << "%";
    }
    // If there are no dag blocks in the period, skip it
    if (db_rlp.itemCount() > 2 && db_rlp[2].itemCount() == 0) {
      continue;
    }
    // skip if the period data is already in the new format
    try {
      auto period_data = ::taraxa::PeriodData::FromOldPeriodData(db_rlp);
      db_->insert(batch, DbStorage::Columns::period_data, period, period_data.rlp());
    } catch (const dev::RLPException& e) {
      continue;
    }
    if (batch.GetDataSize() > max_size) {
      db_->commitWriteBatch(batch);
    }
  }
  db_->commitWriteBatch(batch);
  db_->compactColumn(DbStorage::Columns::period_data);
}
}  // namespace taraxa::storage::migration