#include "storage/migration/migration_manager.hpp"

#include "storage/migration/transaction_receipts_by_period.hpp"

namespace taraxa::storage::migration {

Manager::Manager(std::shared_ptr<DbStorage> db)
    : db_(db), logger_(logger::Logging::get().CreateChannelLogger("MIGRATIONS")) {}
void Manager::applyMigration(std::shared_ptr<migration::Base> m) {
  if (m->isApplied()) {
    logger_->info("Skip \"{}\" migration. It was already applied", m->id());
    return;
  }

  if (db_->getMajorVersion() != m->dbVersion()) {
    logger_->info(
        "Skip \"{}\" migration as it was made for different major db version {}, current db major version {}. "
        "Migration can be removed from the code",
        m->id(), m->dbVersion(), db_->getMajorVersion());
    return;
  }

  logger_->info("Applying migration {}", m->id());
  m->apply(logger_);
  logger_->info("Migration applied {}", m->id());
}

void Manager::applyAll() {
  for (const auto& m : migrations_) {
    applyMigration(m);
  }
}

void Manager::applyReceiptsByPeriod() { applyMigration(std::make_shared<TransactionReceiptsByPeriod>(db_)); }

}  // namespace taraxa::storage::migration