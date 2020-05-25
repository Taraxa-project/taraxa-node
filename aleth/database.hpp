#ifndef TARAXA_NODE_ALETH_DATABASE_ADAPTER_HPP_
#define TARAXA_NODE_ALETH_DATABASE_ADAPTER_HPP_

#include <libdevcore/db.h>

#include "../db_storage.hpp"

namespace taraxa::aleth::database {
using namespace std;
using namespace dev::db;
using dev::db::Slice;

struct Database : virtual DatabaseFace {
 private:
  shared_ptr<DbStorage> db_;
  DbStorage::Column column_;
  DbStorage::BatchPtr batch_;

 public:
  Database(decltype(db_) db, decltype(column_) column)
      : db_(db), column_(column) {}

  void setBatch(DbStorage::BatchPtr batch);

  string lookup(Slice _key) const override;
  bool exists(Slice _key) const override;
  void insert(Slice _key, Slice _value) override;
  void kill(Slice _key) override;
  void forEach(function<bool(Slice, Slice)> f) const override;
};

}  // namespace taraxa::aleth::database

namespace taraxa::aleth {
using database::Database;
}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_ALETH_DATABASE_ADAPTER_HPP_
