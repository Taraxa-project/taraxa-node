//
// Created by JC on 2019-04-26.
//
#include "SimpleDBFace.h"

SimpleDBFace::SimpleDBFace(SimpleDBType dbtype, std::string path):type(dbtype) {
  switch(type) {
    case SimpleDBFace::TaraxaRocksDBKind:
      taraxa_rocks_db = std::make_shared<taraxa::RocksDb>(path);
      break;
    case SimpleDBFace::StateDBKind:
      state = std::make_shared<State>(0, dev::eth::State::openDB(path, TEMP_GENESIS_HASH, dev::WithExisting::Kill),
                                        dev::eth::BaseState::Empty);
      break;
    case SimpleDBFace::OverlayDBKind:
      odb = std::make_shared<OverlayDB>(OverlayDB(dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                                                                          dev::WithExisting::Kill)));
      break;
    default:
      assert(false);
  }
}


bool SimpleDBFace::put(const std::string &key, const std::string &value) {
  bool res = true;
  if(type == TaraxaRocksDBKind) {
    res = taraxa_rocks_db->put(key, value);
  } else if (type == StateDBKind) {
    // TODO: fill in, assert
    //state->setBalance(key, value);
  } else if (type == OverlayDBKind) {
    // TODO:fill in
    //odb->insert(key, value);
  }
  return res;
}

std::string SimpleDBFace::get(const std::string &key) {
  std::string res;
  if(type == TaraxaRocksDBKind) {
    res = taraxa_rocks_db->get(key);
  } else if (type == StateDBKind) {
    // TODO: assert key type
    //return state->balance(key);
  } else if (type == OverlayDBKind) {
    //return odb->lookup(key);
  }
  return res;
}

bool SimpleDBFace::update(const std::string &key, const std::string &value) {
  bool res = true;
  if(type == TaraxaRocksDBKind) {
    res = taraxa_rocks_db->update(key, value);
  } else if (type == StateDBKind) {
    // TODO: assert key type
    //state->setBalance(key, value);
  } else if (type == OverlayDBKind) {
    // TODO:fill in
    //odb->insert(key);
  }
  return res;
}

void SimpleDBFace::commit() {
  switch(type) {
    case SimpleDBFace::TaraxaRocksDBKind:
      // NOP
      return;
    case SimpleDBFace::StateDBKind:
      // TODO: make sure we choose the right behavior
      state->commit(dev::eth::State::CommitBehaviour::KeepEmptyAccounts);
      return;
    case SimpleDBFace::OverlayDBKind:
      odb->commit();
      return;
    default:
      assert(false);
  }
}

std::shared_ptr<SimpleDBFace> SimpleDBFace::createShared(SimpleDBFace::SimpleDBType type, const std::string & path) {
  return std::make_shared<SimpleDBFace>(type, path);
}
