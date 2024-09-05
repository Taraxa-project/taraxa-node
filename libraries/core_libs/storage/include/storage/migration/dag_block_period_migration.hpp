#pragma once
#include "storage/migration/migration_base.hpp"

namespace taraxa::storage::migration {
class DagBlockPeriodMigration : public migration::Base {
 public:
  DagBlockPeriodMigration(std::shared_ptr<DbStorage> db);
  std::string id() override;
  uint32_t dbVersion() override;
  void migrate(logger::Logger& log) override;
};
}  // namespace taraxa::storage::migration