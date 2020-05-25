#include "database.hpp"

#include "util.hpp"

namespace taraxa::aleth::database {

Slice aleth_slice(::rocksdb::Slice const& s) { return {s.data(), s.size()}; }

::rocksdb::Slice db_slice(Slice const& s) { return {s.begin(), s.size()}; }

void Database::setBatch(DbStorage::BatchPtr batch) { batch_ = batch; }

void Database::insert(Slice k, Slice v) {
  db_->batch_put(batch_, column_, db_slice(k), db_slice(v));
}

void Database::kill(Slice k) {
  db_->batch_delete(batch_, column_, db_slice(k));
}

string Database::lookup(Slice _key) const {
  return db_->lookup(db_slice(_key), column_);
}

bool Database::exists(Slice _key) const { return !lookup(_key).empty(); }

void Database::forEach(function<bool(Slice, Slice)> f) const {
  db_->forEach(column_, [&](auto const& k, auto const& v) {
    return f(aleth_slice(k), aleth_slice(v));
  });
}

}  // namespace taraxa::aleth::database