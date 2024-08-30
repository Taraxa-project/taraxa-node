#include "storage/migration/dag_block.hpp"

#include <libdevcore/Common.h>

#include <cstdint>

#include "common/thread_pool.hpp"
#include "common/util.hpp"
#include "pbft/period_data.hpp"

namespace taraxa::storage::migration {

DagBlockData::DagBlockData(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string DagBlockData::id() { return "DagBlockData"; }

uint32_t DagBlockData::dbVersion() { return 1; }

void DagBlockData::migrate(logger::Logger& log) {
  auto orig_col = DbStorage::Columns::period_data;
  auto copied_col = db_->copyColumn(db_->handle(orig_col), orig_col.name() + "-copy");

  if (copied_col == nullptr) {
    LOG(log) << "Migration " << id() << " skipped: Unable to copy " << orig_col.name() << " column";
    return;
  }

  auto it = db_->getColumnIterator(orig_col);
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
  it->SeekToFirst();
  // Get and save data in new format for all blocks
  for (; it->Valid(); it->Next()) {
    uint64_t period;
    memcpy(&period, it->key().data(), sizeof(uint64_t));
    std::string raw = it->value().ToString();
    const auto period_data_old_rlp = dev::RLP(raw);
    auto period_data = ::taraxa::PeriodData::FromOldPeriodData(period_data_old_rlp);
    db_->insert(batch, copied_col.get(), period, period_data.rlp());

    if (batch.GetDataSize() > max_size) {
      db_->commitWriteBatch(batch);
    }

    auto percentage = (period - start_period) * 100 / diff;
    if (percentage > curr_progress) {
      curr_progress = percentage;
      LOG(log) << "Migration " << id() << " progress " << curr_progress << "%";
    }
  }
  db_->commitWriteBatch(batch);

  db_->replaceColumn(orig_col, std::move(copied_col));
}
}  // namespace taraxa::storage::migration