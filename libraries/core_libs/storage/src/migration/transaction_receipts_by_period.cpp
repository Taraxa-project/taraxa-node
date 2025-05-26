#include "storage/migration/transaction_receipts_by_period.hpp"

#include <libdevcore/Common.h>
#include <libdevcore/CommonData.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <chrono>

#include "common/thread_pool.hpp"
#include "storage/storage.hpp"
namespace taraxa::storage::migration {

TransactionReceiptsByPeriod::TransactionReceiptsByPeriod(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string TransactionReceiptsByPeriod::id() { return "TransactionReceiptsByPeriod"; }

uint32_t TransactionReceiptsByPeriod::dbVersion() { return 1; }

void TransactionReceiptsByPeriod::migrate(logger::Logger& logger) {
  auto orig_col = DbStorage::Columns::final_chain_receipt_by_trx_hash;
  auto target_col = DbStorage::Columns::final_chain_receipt_by_period;
  db_->read_options_.async_io = true;
  db_->read_options_.verify_checksums = false;
  auto it = db_->getColumnIterator(DbStorage::Columns::period_data);
  {
    auto target_it = db_->getColumnIterator(target_col);
    target_it->SeekToFirst();

    uint64_t start_period;
    if (!target_it->Valid()) {
      it->SeekToLast();
      if (!it->Valid()) {
        logger->info("No period data found");
        return;
      }
      memcpy(&start_period, it->key().data(), sizeof(uint64_t));
      logger->info("Migrating from period {}", start_period);
    } else {
      memcpy(&start_period, target_it->key().data(), sizeof(uint64_t));
      logger->info("Migrating from period {}", start_period);
      // Start from the smallest migrated period
      it->SeekForPrev(target_it->key());
      it->Prev();
    }
  }
  // const size_t batch_size = 50 * 1024 * 1024;  // 50MB
  // Get and save data in new format for all blocks
  util::ThreadPool pool(std::thread::hardware_concurrency());
  for (; it->Valid(); it->Prev()) {
    uint64_t period;
    memcpy(&period, it->key().data(), sizeof(uint64_t));
    if (period % 10000 == 0) {
      logger->info("Migrating period {}", period);
    }
    const auto transactions = db_->transactionsFromPeriodDataRlp(period, dev::RLP(it->value().ToString()));
    if (transactions.empty()) {
      continue;
    }

    bool commit = false;
    std::vector<std::string> receipts(transactions.size());
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      pool.post([this, period, i, &orig_col, &transactions, &commit, &receipts]() {
        auto hash = transactions[i]->getHash();
        const auto& receipt_raw = db_->lookup(hash, orig_col);
        if (receipt_raw.empty()) {
          throw std::runtime_error("Transaction receipt not found for transaction " + hash.hex() + " in period " +
                                   std::to_string(period) + " " + std::to_string(i));
        }
        commit = true;
        receipts[i] = receipt_raw;
      });
    }

    while (pool.num_pending_tasks() > 0) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }
    if (!commit) {
      break;
    }
    dev::RLPStream receipts_rlp;
    receipts_rlp.appendList(transactions.size());
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      db_->remove(batch_, orig_col, transactions[i]->getHash());
      receipts_rlp.appendRaw(receipts[i]);
    }

    db_->insert(batch_, target_col, period, receipts_rlp.invalidate());
  }
  db_->commitWriteBatch(batch_);
  db_->compactColumn(target_col);
}

}  // namespace taraxa::storage::migration