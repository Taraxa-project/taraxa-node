#include "storage/migration/migration_manager.hpp"

#include "storage/migration/final_chain_header.hpp"
#include "storage/migration/period_dag_blocks.hpp"
#include "storage/migration/transaction_period.hpp"
namespace taraxa::storage::migration {

Manager::Manager(std::shared_ptr<DbStorage> db, const addr_t& node_addr) : db_(db) {
  registerMigration<PeriodDagBlocks>();
  registerMigration<FinalChainHeader>();
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
void Manager::applyTransactionPeriod() { applyMigration(std::make_shared<TransactionPeriod>(db_)); }

}  // namespace taraxa::storage::migration