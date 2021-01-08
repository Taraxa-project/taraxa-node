#ifndef TARAXA_NODE_ALETH_DATABASE_ADAPTER_HPP_
#define TARAXA_NODE_ALETH_DATABASE_ADAPTER_HPP_

#include "../db_storage.hpp"
#include "db.h"
#include "types.hpp"

namespace taraxa::aleth {

struct Database : virtual DatabaseFace {
  virtual ~Database() {}
  virtual void setBatch(DbStorage::BatchPtr batch) = 0;
};

std::unique_ptr<Database> NewDatabase(std::shared_ptr<DbStorage> db, DbStorage::Column column);

}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_ALETH_DATABASE_ADAPTER_HPP_
