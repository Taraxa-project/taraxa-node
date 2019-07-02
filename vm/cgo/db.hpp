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
using Slice = dev::db::Slice;

namespace exports {
using Error = taraxa_cgo_ethdb_Error;
using Key = taraxa_cgo_ethdb_Key;
using Value = taraxa_cgo_ethdb_Value;
using ValueAndErr = taraxa_cgo_ethdb_ValueAndErr;
using BoolAndErr = taraxa_cgo_ethdb_BoolAndErr;

struct Batch {
  const taraxa_cgo_ethdb_Batch c_impl_ = {
      this,
      [](auto self) { delete decltype(this)(self); },
      [](auto self, auto key, auto value) {
        return decltype(this)(self)->Put(key, value);
      },
      [](auto self, auto key) { return decltype(this)(self)->Delete(key); },
      [](auto self) { return decltype(this)(self)->Write(); },
      [](auto self) { return decltype(this)(self)->Reset(); },
  };

  auto cImpl() { return const_cast<taraxa_cgo_ethdb_Batch*>(&c_impl_); }

  virtual ~Batch() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Delete(Key key) = 0;
  virtual Error Write() = 0;
  virtual void Reset() = 0;
};

struct Database {
  const taraxa_cgo_ethdb_Database c_impl_ = {
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

  auto cImpl() { return const_cast<taraxa_cgo_ethdb_Database*>(&c_impl_); }

  virtual ~Database() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Delete(Key key) = 0;
  virtual ValueAndErr Get(Key key) = 0;
  virtual BoolAndErr Has(Key key) = 0;
  virtual void Close() = 0;
  virtual Batch* NewBatch() = 0;
};

class AlethBatch : public Batch {
  const shared_ptr<DatabaseFace> database;
  unique_ptr<WriteBatchFace> batch;

 public:
  explicit AlethBatch(const decltype(database)& db)
      : database(db), batch(db->createWriteBatch()) {}

  Error Put(Key key, Value value) override {
    batch->insert({key.offset, key.size}, {value.offset, value.size});
    return {};
  }

  Error Delete(Key key) override {
    batch->kill({key.offset, key.size});
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
    database->insert({key.offset, key.size}, {value.offset, value.size});
    return {};
  }

  Error Delete(Key key) override {
    database->kill({key.offset, key.size});
    return {};
  }

  ValueAndErr Get(Key key) override {
    const auto& value = database->lookup({key.offset, key.size});
    const auto& valueSize = value.size();
    auto valueCpy = (char*)malloc(sizeof(char) * valueSize);
    value.copy(valueCpy, valueSize);
    return {{valueCpy, valueSize}};
  }

  BoolAndErr Has(Key key) override {
    return {database->exists({key.offset, key.size})};
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
