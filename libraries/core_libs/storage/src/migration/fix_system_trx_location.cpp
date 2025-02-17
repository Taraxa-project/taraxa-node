#include "storage/migration/fix_system_trx_location.hpp"

#include "transaction/receipt.hpp"

namespace taraxa::storage::migration {

FixSystemTrxLocation::FixSystemTrxLocation(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string FixSystemTrxLocation::id() { return "FixSystemTrxLocation"; }

uint32_t FixSystemTrxLocation::dbVersion() { return 1; }

void FixSystemTrxLocation::migrate(logger::Logger&) {
  auto orig_col = DbStorage::Columns::system_transaction;

  auto it = db_->getColumnIterator(orig_col);
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  for (; it->Valid(); it->Next()) {
    auto trx_hash = dev::h256(dev::asBytes(it->key().ToString()));
    auto loc = db_->getTransactionLocation(trx_hash);
    loc->position -= 1;
    db_->addTransactionLocationToBatch(batch_, trx_hash, loc->period, loc->position, true);
  }
}

}  // namespace taraxa::storage::migration
