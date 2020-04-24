#include "database_adapter.hpp"

#include "util.hpp"

namespace taraxa::eth::database_adapter {
using std::shared_lock;

Slice aleth_slice(rocksdb::Slice const& s) { return {s.data(), s.size()}; }

rocksdb::Slice db_slice(Slice const& s) { return {s.begin(), s.size()}; }

OnceOutOfScope DatabaseAdapter::setBatch(DbStorage::BatchPtr batch) {
  unique_lock l(batch_mu_);
  assert(!batch_);
  batch_ = batch;
  return OnceOutOfScope([this] {
    unique_lock l(batch_mu_);
    batch_ = nullptr;
  });
}

void DatabaseAdapter::insert(Slice k, Slice v) {
  unique_lock l(batch_mu_);
  db_->batch_put(batch_, column_, db_slice(k), db_slice(v));
}

void DatabaseAdapter::kill(Slice k) {
  unique_lock l(batch_mu_);
  db_->batch_delete(batch_, column_, db_slice(k));
}

string DatabaseAdapter::lookup(Slice _key) const {
  return db_->lookup(db_slice(_key), column_);
}

bool DatabaseAdapter::exists(Slice _key) const { return !lookup(_key).empty(); }

void DatabaseAdapter::forEach(function<bool(Slice, Slice)> f) const {
  db_->forEach(column_, [&](auto const& k, auto const& v) {
    return f(aleth_slice(k), aleth_slice(v));
  });
}

}  // namespace taraxa::eth::database_adapter