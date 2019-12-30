#ifndef TARAXA_NODE_TRX_ENGINE_DB_CGO_DB_HPP_
#define TARAXA_NODE_TRX_ENGINE_DB_CGO_DB_HPP_

extern "C" {
#include "taraxa/trx_engine/db/cgo/index.h"
}
#include <libdevcore/Log.h>
#include <libdevcore/db.h>

#include <cstring>

#include "util/eth.hpp"

namespace taraxa::trx_engine::db::cgo {
using namespace dev;
using namespace dev::db;
using namespace std;
using namespace util::eth;

using SliceType = taraxa_cgo_ethdb_SliceType;
using SliceSize = taraxa_cgo_ethdb_SliceSize;
using Slice = taraxa_cgo_ethdb_Slice;
using Error = taraxa_cgo_ethdb_Error;
using Key = taraxa_cgo_ethdb_Key;
using Value = taraxa_cgo_ethdb_Value;
using ValueAndErr = taraxa_cgo_ethdb_ValueAndErr;
using CgoBatch = taraxa_cgo_ethdb_Batch;
using CgoDatabase = taraxa_cgo_ethdb_Database;

struct BatchAdapter {
  CgoBatch c_self = {
      this,
      [](auto self) { delete decltype(this)(self); },
      [](auto self, auto key, auto value) {
        return decltype(this)(self)->Put(key, value);
      },
      [](auto self) { return decltype(this)(self)->Write(); },
  };

  virtual ~BatchAdapter() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual Error Write() = 0;
};

struct DatabaseAdapter {
  CgoDatabase c_self = {
      this,
      [](auto self) { delete decltype(this)(self); },
      [](auto self, auto key, auto value) {
        return decltype(this)(self)->Put(key, value);
      },
      [](auto self, auto key) { return decltype(this)(self)->Get(key); },
      [](auto self) { return &decltype(this)(self)->NewBatch()->c_self; },
  };

  virtual ~DatabaseAdapter() = default;
  virtual Error Put(Key key, Value value) = 0;
  virtual ValueAndErr Get(Key key) = 0;
  virtual BatchAdapter* NewBatch() = 0;
};

inline auto alethSlice(const Slice& s) {
  return dev::db::Slice(s.offset, s.size);
}

class AlethBatchAdapter : public virtual BatchAdapter {
  DatabaseFace* database;
  unique_ptr<WriteBatchFace> batch;

 public:
  explicit AlethBatchAdapter(decltype(database) const& db)
      : database(db), batch(db->createWriteBatch()) {}

  Error Put(Key key, Value value) override {
    batch->insert(alethSlice(key), alethSlice(value));
    return {};
  }

  Error Write() override {
    database->commit(move(batch));
    return {};
  }
};

class AlethDatabaseAdapter : public virtual DatabaseAdapter {
  DatabaseFace* database;

 public:
  explicit AlethDatabaseAdapter(decltype(database) db) : database(move(db)) {}

  Error Put(Key key, Value value) override {
    database->insert(alethSlice(key), alethSlice(value));
    return {};
  }

  ValueAndErr Get(Key key) override {
    auto value = database->lookup(alethSlice(key));
    SliceSize size = value.size();
    auto offset = (char*)taraxa_cgo_malloc(sizeof(char) * size);
    memcpy(offset, value.data(), size);
    return {{offset, size}};
  }

  BatchAdapter* NewBatch() override { return new AlethBatchAdapter(database); }
};

}  // namespace taraxa::trx_engine::db::cgo

#endif
