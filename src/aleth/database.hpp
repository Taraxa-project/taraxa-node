#pragma once

#include <libdevcore/db.h>

#include "storage/db_storage.hpp"

namespace taraxa::aleth {

struct Database : virtual dev::db::DatabaseFace {
  virtual ~Database() {}
  virtual void setBatch(DbStorage::BatchPtr batch) = 0;
};

std::unique_ptr<Database> NewDatabase(std::shared_ptr<DbStorage> db, DbStorage::Column column);

}  // namespace taraxa::aleth
