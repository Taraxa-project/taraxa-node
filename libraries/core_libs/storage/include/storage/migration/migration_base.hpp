#pragma once
#include "storage/storage.hpp"

namespace taraxa::storage::migration {
class Base {
 public:
  Base(std::shared_ptr<DbStorage> db) : db_(std::move(db)), batch_(db_->createWriteBatch()) {}
  virtual ~Base() = default;
  virtual std::string id() = 0;
  // We need to specify version here, so in case of major version change(db reindex) we won't apply unneeded migrations
  virtual uint32_t dbVersion() = 0;

  bool isApplied() { return db_->lookup_int<bool>(id(), DbStorage::Columns::migrations).has_value(); }

  void apply(logger::Logger& log) {
    migrate(log);
    setApplied();
    db_->commitWriteBatch(batch_);
  }

 protected:
  // Method with custom logic. All db changes should be made using `batch_`
  virtual void migrate(logger::Logger& log) = 0;

  void setApplied() { db_->insert(batch_, DbStorage::Columns::migrations, id(), true); }

  std::shared_ptr<DbStorage> db_;
  Batch batch_;
};
}  // namespace taraxa::storage::migration