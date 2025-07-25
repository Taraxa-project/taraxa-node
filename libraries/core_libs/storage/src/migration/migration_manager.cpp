#include "storage/migration/migration_manager.hpp"

#include "storage/migration/fix_system_trx_location.hpp"
#include "storage/migration/transaction_receipts_by_period.hpp"

namespace taraxa::storage::migration {

Manager::Manager(std::shared_ptr<DbStorage> db, const addr_t& node_addr) : db_(db) {
  registerMigration<FixSystemTrxLocation>();
  LOG_OBJECTS_CREATE("MIGRATIONS");
}
void Manager::applyMigration(std::shared_ptr<migration::Base> m) {
  if (m->isApplied()) {
    LOG(log_si_) << "Skip \"" << m->id() << "\" migration. It was already applied";
    return;
  }

  if (db_->getMajorVersion() != m->dbVersion()) {
    LOG(log_si_) << "Skip \"" << m->id() << "\" migration as it was made for different major db version "
                 << m->dbVersion() << ", current db major version " << db_->getMajorVersion()
                 << ". Migration can be removed from the code";
    return;
  }

  LOG(log_si_) << "Applying migration " << m->id();
  m->apply(log_si_);
  LOG(log_si_) << "Migration applied " << m->id();
}

void Manager::applyAll() {
  for (const auto& m : migrations_) {
    applyMigration(m);
  }
}

void Manager::applyReceiptsByPeriod() { applyMigration(std::make_shared<TransactionReceiptsByPeriod>(db_)); }

}  // namespace taraxa::storage::migration