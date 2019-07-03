#ifndef TARAXA_NODE_CGO_DB_HPP
#define TARAXA_NODE_CGO_DB_HPP

#include <libdevcore/Log.h>
#include <libdevcore/db.h>
#include <cstring>
#include "generated/taraxa_vm_cgo.h"
#include "util_eth.hpp"

namespace taraxa::vm::cgo::db::__impl__ {
using namespace dev;
using namespace dev::db;
using namespace std;
using namespace util::eth;

namespace exports {
using SliceType = taraxa_cgo_ethdb_SliceType;
using SliceSize = taraxa_cgo_ethdb_SliceSize;
using Slice = taraxa_cgo_ethdb_Slice;
using Error = taraxa_cgo_ethdb_Error;
using Key = taraxa_cgo_ethdb_Key;
using Value = taraxa_cgo_ethdb_Value;
using ValueAndErr = taraxa_cgo_ethdb_ValueAndErr;
using BoolAndErr = taraxa_cgo_ethdb_BoolAndErr;
using CgoBatch = taraxa_cgo_ethdb_Batch;
using CgoDatabase = taraxa_cgo_ethdb_Database;

struct Batch {
  const CgoBatch c_impl_ = {
      this,
      [](auto self) { delete decltype(this)(self); },
      [](auto self, auto key, auto value) {
        return decltype(this)(self)->Put(key, value);
      },
      [](auto self, auto key) { return decltype(this)(self)->Delete(key); },
      [](auto self) { return decltype(this)(self)->Write(); },
      [](auto self) { return decltype(this)(self)->Reset(); },
  };

  auto cImpl() { return const_cast<CgoBatch*>(&c_impl_); }

  virtual ~Batch() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Delete(Key key) = 0;
  virtual Error Write() = 0;
  virtual void Reset() = 0;
};

struct Database {
  const CgoDatabase c_impl_ = {
      this,
      [](auto self) { delete decltype(this)(self); },
      [](auto self, auto key, auto value) {
        return decltype(this)(self)->Put(key, value);
      },
      [](auto self, auto key) { return decltype(this)(self)->Delete(key); },
      [](auto self, auto key) { return decltype(this)(self)->Get(key); },
      [](auto self, auto key) { return decltype(this)(self)->Has(key); },
      [](auto self) { return decltype(this)(self)->Close(); },
      [](auto self) { return decltype(this)(self)->NewBatch()->cImpl(); },
  };

  auto cImpl() { return const_cast<CgoDatabase*>(&c_impl_); }

  virtual ~Database() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Delete(Key key) = 0;
  virtual ValueAndErr Get(Key key) = 0;
  virtual BoolAndErr Has(Key key) = 0;
  virtual void Close() = 0;
  virtual Batch* NewBatch() = 0;
};

inline auto alethSlice(const Slice& s) {
  return dev::db::Slice(s.offset, s.size);
}

class AlethBatch : public Batch {
  const shared_ptr<DatabaseFace> database;
  unique_ptr<WriteBatchFace> batch;

 public:
  explicit AlethBatch(const decltype(database)& db)
      : database(db), batch(db->createWriteBatch()) {}

  Error Put(Key key, Value value) override {
    batch->insert(alethSlice(key), alethSlice(value));
    return {};
  }

  Error Delete(Key key) override {
    batch->kill(alethSlice(key));
    return {};
  }

  Error Write() override {
    database->commit(move(batch));
    return {};
  }

  void Reset() override { batch = database->createWriteBatch(); }
};

class AlethDatabase : public Database {
  shared_ptr<DatabaseFace> database;

 public:
  explicit AlethDatabase(decltype(database) db) : database(move(db)) {}

  Error Put(Key key, Value value) override {
    database->insert(alethSlice(key), alethSlice(value));
    return {};
  }

  Error Delete(Key key) override {
    database->kill(alethSlice(key));
    return {};
  }

  ValueAndErr Get(Key key) override {
    const auto& value = database->lookup(alethSlice(key));
    const SliceSize& size = value.size();
    auto offset = (char*)taraxa_cgo_malloc(sizeof(char) * size);
    memcpy(offset, value.data(), size);
    return {{offset, size}};
  }

  BoolAndErr Has(Key key) override {
    return {database->exists(alethSlice(key))};
  }

  void Close() override {}

  Batch* NewBatch() override { return new AlethBatch(database); }
};

}  // namespace exports
}  // namespace taraxa::vm::cgo::db::__impl__

namespace taraxa::vm::cgo::db {
using namespace __impl__::exports;
}

#endif  // TARAXA_NODE_CGO_DB_HPP
