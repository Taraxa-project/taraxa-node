#include "storage/migration/migration_manager.hpp"

namespace taraxa::storage::migration {
Manager::Manager(std::shared_ptr<DbStorage> db, const addr_t& node_addr) : db_(db) { LOG_OBJECTS_CREATE("MIGRATIONS"); }

void Manager::applyAll() {
  for (const auto& m : migrations_) {
    if (!m->isApplied()) {
      LOG(log_nf_) << "Applying migration " << m->id();
      m->apply();
      LOG(log_nf_) << "Migration applied " << m->id();
    }
  }
}
}  // namespace taraxa::storage::migration