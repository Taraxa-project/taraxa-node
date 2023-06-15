#include "storage/migration/migration_manager.hpp"

#include "storage/migration/period_data.hpp"
#include "storage/migration/transaction_hashes.hpp"

namespace taraxa::storage::migration {
Manager::Manager(std::shared_ptr<DbStorage> db, const addr_t& node_addr) : db_(db) {
  LOG_OBJECTS_CREATE("MIGRATIONS");
  registerMigration<migration::TransactionHashes>();
  registerMigration<migration::PeriodData>();
}

void Manager::applyAll() {
  for (const auto& m : migrations_) {
    if (m->isApplied()) {
      LOG(log_si_) << "Skip \"" << m->id() << "\" migration. It was already applied";
      continue;
    }

    if (db_->getMajorVersion() != m->dbVersion()) {
      LOG(log_si_) << "Skip \"" << m->id() << "\" migration as it was made for different major db version "
                   << m->dbVersion() << ", current db major version " << db_->getMajorVersion()
                   << ". Migration can be removed from the code";
      continue;
    }

    LOG(log_si_) << "Applying migration " << m->id();
    m->apply(log_si_);
    LOG(log_si_) << "Migration applied " << m->id();
  }
}
}  // namespace taraxa::storage::migration