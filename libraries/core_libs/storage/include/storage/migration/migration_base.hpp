#pragma once
#include "storage/storage.hpp"

namespace taraxa::storage::migration {
class Base {
 public:
  Base(std::shared_ptr<DbStorage> db) : db_(std::move(db)) {}
  virtual ~Base() = default;
  virtual std::string id() = 0;
  virtual void migrate() = 0;
  void apply() {
    migrate();
    setApplied();
  }
  void setApplied() { db_->insert(DB::Columns::migrations, id(), true); }
  bool isApplied() { return db_->lookup_int<bool>(id(), DB::Columns::migrations).has_value(); }

 protected:
  std::shared_ptr<DbStorage> db_;
};
}  // namespace taraxa::storage::migration