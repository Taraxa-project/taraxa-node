#ifndef TARAXA_NODE_ETH_DATABASE_ADAPTER_HPP_
#define TARAXA_NODE_ETH_DATABASE_ADAPTER_HPP_

#include <libdevcore/db.h>

#include <shared_mutex>

#include "db_storage.hpp"
#include "util/once_out_of_scope.hpp"

namespace taraxa::eth::database_adapter {
using dev::db::DatabaseFace;
using dev::db::Slice;
using rocksdb::ColumnFamilyHandle;
using rocksdb::DB;
using rocksdb::ReadOptions;
using rocksdb::WriteBatch;
using rocksdb::WriteOptions;
using std::atomic;
using std::function;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using util::once_out_of_scope::OnceOutOfScope;

struct DatabaseAdapter : virtual DatabaseFace {
 private:
  shared_ptr<DbStorage> db_;
  DbStorage::Column column_;
  // TODO optimistic lock
  mutex batch_mu_;
  DbStorage::BatchPtr batch_;

 public:
  explicit DatabaseAdapter(decltype(db_) const& db, decltype(column_) column)
      : db_(db), column_(column) {}

  OnceOutOfScope setBatch(DbStorage::BatchPtr batch);

  virtual ~DatabaseAdapter() override = default;
  string lookup(Slice _key) const override;
  bool exists(Slice _key) const override;
  void insert(Slice _key, Slice _value) override;
  void kill(Slice _key) override;
  void forEach(function<bool(Slice, Slice)> f) const override;
};

}  // namespace taraxa::eth::database_adapter

#endif  // TARAXA_NODE_ETH_DATABASE_ADAPTER_HPP_
