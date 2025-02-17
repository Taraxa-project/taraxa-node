#include "storage/migration/transaction_receipts_by_period.hpp"

#include <libdevcore/Common.h>
#include <libdevcore/CommonData.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include "common/thread_pool.hpp"
#include "storage/storage.hpp"

namespace taraxa::storage::migration {

TransactionReceiptsByPeriod::TransactionReceiptsByPeriod(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string TransactionReceiptsByPeriod::id() { return "TransactionReceiptsByPeriod"; }

uint32_t TransactionReceiptsByPeriod::dbVersion() { return 1; }

void TransactionReceiptsByPeriod::migrate(logger::Logger& log) {
  auto orig_col = DbStorage::Columns::final_chain_receipt_by_trx_hash;
  auto target_col = DbStorage::Columns::final_chain_receipt_by_period;
  db_->read_options_.async_io = true;
  db_->read_options_.verify_checksums = false;
  auto it = db_->getColumnIterator(DbStorage::Columns::period_data);
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  uint64_t start_period, end_period;
  memcpy(&start_period, it->key().data(), sizeof(uint64_t));

  it->SeekToLast();
  if (!it->Valid()) {
    throw std::runtime_error("Something wrong, invalid period_data iterator");
  }
  memcpy(&end_period, it->key().data(), sizeof(uint64_t));
  it->SeekToFirst();
  // log count of the elements in the column
  const auto diff = (end_period - start_period) ? (end_period - start_period) : 1;
  LOG(log) << "Migrating " << diff << " periods";
  uint64_t curr_progress = 0;

  const size_t batch_size = 500000000;
  util::ThreadPool pool(4);
  // Get and save data in new format for all blocks
  for (; it->Valid(); it->Next()) {
    uint64_t period;
    memcpy(&period, it->key().data(), sizeof(uint64_t));

    auto percentage = (period - start_period) * 100 / diff;
    if (percentage > curr_progress) {
      curr_progress = percentage;
      LOG(log) << "Migration " << id() << " progress " << curr_progress << "%";
    }

    // auto now = std::chrono::steady_clock::now();
    const auto transactions = db_->transactionsFromPeriodDataRlp(period, dev::RLP(it->value().ToString()));
    if (transactions.empty()) {
      continue;
    }

    bool commit = false;
    dev::RLPStream receipts_rlp;
    receipts_rlp.appendList(transactions.size());
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      auto hash = transactions[i]->getHash();
      const auto& receipt_raw = db_->lookup(hash, orig_col);
      if (receipt_raw.empty()) {
        auto header_exists = db_->exist(period, DbStorage::Columns::final_chain_blk_by_number);
        if (!header_exists) {
          break;
        }
        throw std::runtime_error("Transaction receipt not found for transaction " + hash.hex() + " in period " +
                                 std::to_string(period) + " " + std::to_string(i));
      }
      commit = true;
      receipts_rlp.appendRaw(receipt_raw);
    }

    if (!commit) {
      break;
    }
    db_->insert(batch_, target_col, period, receipts_rlp.invalidate());

    if (batch_.GetDataSize() > batch_size) {
      db_->commitWriteBatch(batch_);
    }
  }
  db_->commitWriteBatch(batch_);
  // db_->deleteColumnData(orig_col);
  db_->compactColumn(target_col);
}

}  // namespace taraxa::storage::migration