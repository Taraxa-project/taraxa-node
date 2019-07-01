#ifndef TARAXA_NODE_CGO_DB_HPP
#define TARAXA_NODE_CGO_DB_HPP

#include <libdevcore/Log.h>
#include <libdevcore/db.h>
#include <cstring>
#include "generated/taraxa_vm_cgo.h"

namespace taraxa::vm::cgo::db::__impl__ {
using namespace dev;
using namespace dev::db;
using namespace std;
using Slice = dev::db::Slice;

namespace exports {
using Error = taraxa_cgo_ethdb_Error;
using Key = taraxa_cgo_ethdb_Key;
using Value = taraxa_cgo_ethdb_Value;
using ValueAndErr = taraxa_cgo_ethdb_ValueAndErr;
using BoolAndErr = taraxa_cgo_ethdb_BoolAndErr;

struct Batch {
  virtual ~Batch() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Delete(Key key) = 0;
  virtual Error Write() = 0;
  virtual void Reset() = 0;

  auto newCgoBatch(decltype(taraxa_cgo_ethdb_Batch::Free) deleter = nullptr) {
    if (!deleter) {
      deleter = [](auto self) { delete decltype(this)(self); };
    }
    return new taraxa_cgo_ethdb_Batch{
        this,
        deleter,
        [](auto self, auto key, auto value) {
          return decltype(this)(self)->Put(key, value);
        },
        [](auto self, auto key) { return decltype(this)(self)->Delete(key); },
        [](auto self) { return decltype(this)(self)->Write(); },
        [](auto self) { return decltype(this)(self)->Reset(); },
    };
  }
};

struct Database {
  virtual ~Database() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Delete(Key key) = 0;
  virtual ValueAndErr Get(Key key) = 0;
  virtual BoolAndErr Has(Key key) = 0;
  virtual void Close() = 0;
  virtual Batch* NewBatch() = 0;

  auto newCgoDB(decltype(taraxa_cgo_ethdb_Database::Free) deleter = nullptr) {
    if (!deleter) {
      deleter = [](auto self) { delete decltype(this)(self); };
    }
    return new taraxa_cgo_ethdb_Database{
        this,
        deleter,
        [](auto self, auto key, auto value) {
          return decltype(this)(self)->Put(key, value);
        },
        [](auto self, auto key) { return decltype(this)(self)->Delete(key); },
        [](auto self, auto key) { return decltype(this)(self)->Get(key); },
        [](auto self, auto key) { return decltype(this)(self)->Has(key); },
        [](auto self) { return decltype(this)(self)->Close(); },
        [](auto self) {
          return decltype(this)(self)->NewBatch()->newCgoBatch();
        },
    };
  }

};

class AlethBatch : public Batch {
  const shared_ptr<DatabaseFace> database;
  unique_ptr<WriteBatchFace> batch;

 public:
  explicit AlethBatch(const decltype(database)& db)
      : database(db), batch(db->createWriteBatch()) {}

  Error Put(Key key, Value value) override {
    batch->insert(Slice(key), Slice(value));
    return nullptr;
  }

  Error Delete(Key key) override {
    batch->kill(Slice(key));
    return nullptr;
  }

  Error Write() override {
    database->commit(move(batch));
    return nullptr;
  }

  void Reset() override { batch = database->createWriteBatch(); }
};

class AlethDatabase : public Database {
  shared_ptr<DatabaseFace> database;

 public:
  explicit AlethDatabase(decltype(database) db) : database(move(db)) {}

  Error Put(Key key, Value value) override {
    database->insert(Slice(key), Slice(value));
    return nullptr;
  }

  Error Delete(Key key) override {
    database->kill(Slice(key));
    return nullptr;
  }

  ValueAndErr Get(Key key) override {
    return {database->lookup(Slice(key)).c_str(), nullptr};
  }

  BoolAndErr Has(Key key) override {
    return {database->exists(Slice(key)), nullptr};
  }

  void Close() override {
    //    database->
  }

  Batch* NewBatch() override { return new AlethBatch(database); }
};

}  // namespace exports
}  // namespace taraxa::vm::cgo::db::__impl__

namespace taraxa::vm::cgo::db {
using namespace __impl__::exports;
}

#endif  // TARAXA_NODE_CGO_DB_HPP
