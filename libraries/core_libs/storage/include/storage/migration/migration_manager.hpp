#pragma once
#include "storage/migration/migration_base.hpp"

namespace taraxa::storage::migration {
class Manager {
 public:
  explicit Manager(std::shared_ptr<DbStorage> db, const addr_t& node_addr = {});
  template <typename T>
  void registerMigration() {
    migrations_.push_back(std::make_shared<T>(db_));
  }
  void applyAll();

  void applyReceiptsByPeriod();

 private:
  void applyMigration(std::shared_ptr<migration::Base> m);
  std::shared_ptr<DbStorage> db_;
  std::vector<std::shared_ptr<migration::Base>> migrations_;
  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa::storage::migration