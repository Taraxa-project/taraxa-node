#include "database_adapter.hpp"

#include "util.hpp"

namespace taraxa::eth::database_adapter {
using std::shared_lock;

Slice aleth_slice(rocksdb::Slice const& s) { return {s.data(), s.size()}; }

rocksdb::Slice db_slice(Slice const& s) { return {s.begin(), s.size()}; }

void DatabaseAdapter::Batch::insert(Slice _key, Slice _value) {
  ops_[_key.toString()] = _value.toString();
}

void DatabaseAdapter::Batch::kill(Slice _key) { ops_[_key.toString()] = ""; }

DatabaseAdapter::TransactionScope DatabaseAdapter::setMasterBatch(
    DbStorage::BatchPtr const& master_batch) {
  unique_lock l(set_master_batch_mu_);
  assert(!master_batch_);
  master_batch_ = master_batch;
  return u_ptr(new OnceOutOfScope([this] {
    unique_lock l(set_master_batch_mu_);
    master_batch_ = nullptr;
  }));
}

string DatabaseAdapter::lookup(Slice _key) const {
  return db_->lookup(db_slice(_key), column_);
}

bool DatabaseAdapter::exists(Slice _key) const { return !lookup(_key).empty(); }

void DatabaseAdapter::insert(Slice _key, Slice _value) {
  db_->insert(column_, db_slice(_key), db_slice(_value));
}

void DatabaseAdapter::kill(Slice _key) { db_->remove(db_slice(_key), column_); }

unique_ptr<WriteBatchFace> DatabaseAdapter::createWriteBatch() const {
  return u_ptr(new Batch);
}

void DatabaseAdapter::commit(unique_ptr<WriteBatchFace> _batch) {
  shared_lock l(set_master_batch_mu_);
  auto batch = dynamic_cast<Batch*>(_batch.get());
  assert(batch);
  auto target_batch = master_batch_ ? master_batch_ : db_->createWriteBatch();
  for (auto& [k, v] : batch->ops_) {
    if (v.empty()) {
      db_->batch_delete(target_batch, column_, k);
    } else {
      db_->batch_put(target_batch, column_, k, v);
    }
  }
  if (!master_batch_) {
    db_->commitWriteBatch(target_batch);
  }
}

void DatabaseAdapter::forEach(function<bool(Slice, Slice)> f) const {
  db_->forEach(column_, [&](auto& k, auto& v) {
    return f(aleth_slice(k), aleth_slice(v));
  });
}

}  // namespace taraxa::eth::database_adapter