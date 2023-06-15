#include "storage/migration/migration_manager.hpp"

#include "storage/migration/transaction_hashes.hpp"

namespace taraxa::storage::migration {
Manager::Manager(std::shared_ptr<DbStorage> db, const addr_t& node_addr) : db_(db) {
  LOG_OBJECTS_CREATE("MIGRATIONS");
  registerMigration<migration::TransactionHashes>();
}

void Manager::applyAll() {
  for (const auto& m : migrations_) {
    if (!m->isApplied()) {
      LOG(log_si_) << "Applying migration " << m->id();
      m->apply(log_si_);
      LOG(log_si_) << "Migration applied " << m->id();
    }
  }
}
}  // namespace taraxa::storage::migration