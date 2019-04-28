//
// Created by JC on 2019-04-26.
//
#ifndef TARAXA_NODE_SIMPLEDBFACE_H
#define TARAXA_NODE_SIMPLEDBFACE_H

#include "rocks_db.hpp"
#include "libdevcore/OverlayDB.h"
#include "libethereum/State.h"
#include "types.hpp"

using State = dev::eth::State;
using h256 = dev::h256;

const h256 TEMP_GENESIS_HASH = h256{}; /* genesisHash, put h256{} for now */

/*
class SimpleTaraxaRocksDBDelegate;
class SimpleStateDBDelegate;
class SimpleOverlayDBDelegate;
*/

class SimpleDBFace {
public:
    /*
    enum SimpleDBType {
        TaraxaRocksDBKind,
        OverlayDBKind,
        StateDBKind
    };
     */
    //static std::shared_ptr<SimpleDBFace> createDelegate(SimpleDBFace::SimpleDBType type, const std::string &path);

    virtual bool put(const std::string &key, const std::string &value) = 0;
    virtual bool update(const std::string &key, const std::string &value) = 0;
    virtual std::string get(const std::string &key) = 0;
    virtual void commit() = 0;
    virtual ~SimpleDBFace() = default;
    //virtual std::shared_ptr<SimpleDBFace> makeShared(const std::string& path);
    //SimpleDBFace(SimpleDBType type, std::string path);
    //std::shared_ptr<SimpleDBFace> findDelegate();
protected:
    //std::shared_ptr<SimpleDBFace> delegate;
    //const SimpleDBType type;
};

/*
class SimpleDBDelegateHolder {
    SimpleDBDelegateHolder() {
      std::shared_ptr<dev::eth::State> state = nullptr;
      std::shared_ptr<taraxa::RocksDb> taraxa_rocks_db = nullptr;
      std::shared_ptr<dev::OverlayDB> odb = nullptr;
    }
    std::shared_ptr<SimpleDBFace> delegate;
    const SimpleDBType type;
};

static std::shared_ptr<SimpleDBFace> createDelegate(const SimpleDBFace::SimpleDBType type, const std::string &path) {
  switch(type) {
    case SimpleDBFace::TaraxaRocksDBKind:
      return SimpleTaraxaRocksDBDelegate::makeShared(path);
    case SimpleDBFace::StateDBKind:
      //return std::make_shared<SimpleStateDBDelegate>(path);
      return SimpleStateDBDelegate::makeShared(path);
    case SimpleDBFace::OverlayDBKind:
      //return std::make_shared<SimpleOverlayDBDelegate>(path);
      return SimpleOverlayDBDelegate::makeShared(path);
    default:
      assert(false);
  }
}
*/
#endif //TARAXA_NODE_TEST_H