#include "database.hpp"

#include "util.hpp"

namespace taraxa::aleth {
using namespace std;
using ::dev::db::Slice;

Slice aleth_slice(::rocksdb::Slice const& s) { return {s.data(), s.size()}; }
::rocksdb::Slice db_slice(Slice const& s) { return {s.begin(), s.size()}; }

struct DatabaseImpl : virtual Database {
  shared_ptr<DbStorage> db_;
  DbStorage::Column column_;
  DbStorage::BatchPtr batch_;

  DatabaseImpl(decltype(db_) db, decltype(column_) column) : db_(db), column_(column) {}

  void setBatch(DbStorage::BatchPtr batch) override { batch_ = move(batch); }

  void insert(Slice k, Slice v) override { db_->batch_put(batch_, column_, db_slice(k), db_slice(v)); }

  string lookup(Slice _key) const override { return db_->lookup(db_slice(_key), column_); }
};

std::unique_ptr<Database> NewDatabase(std::shared_ptr<DbStorage> db, DbStorage::Column column) {
  return u_ptr(new DatabaseImpl(move(db), move(column)));
}

}  // namespace taraxa::aleth