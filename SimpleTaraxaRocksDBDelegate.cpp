//
// Created by JC on 2019-04-26.
//
//#include "SimpleDBFace.h"
/*
class SimpleTaraxaRocksDBDelegate : public SimpleDBFace {
public:
    bool put override (const std::string &key, const std::string &value) {
      return taraxa_rocks_db->put(key, value);
    }
    bool update override(const std::string &key, const std::string &value) {
      return taraxa_rocks_db->update(key, value);
    }
    std::string get override (const std::string &key) {
      return taraxa_rocks_db->get(key);
    }
    void commit() override {
      // NOP
    }
    std::shared_ptr<taraxa::RocksDb> taraxa_rocks_db = nullptr;

    static shared_ptr<SimpleDBFace> makeShared(const std::string& path) {
      return make_shared<SimpleTaraxaRocksDBDelegate>(path);
    }
private:
    SimpleTaraxaRocksDBDelegate(const std::string& path):taraxa_rocks_db(std::make_shared<taraxa::RocksDb>(path)){}
};
*/
#include "SimpleTaraxaRocksDBDelegate.h"
bool SimpleTaraxaRocksDBDelegate::put (const std::string &key, const std::string &value) {
  return taraxa_rocks_db->put(key, value);
}
bool SimpleTaraxaRocksDBDelegate::update (const std::string &key, const std::string &value) {
  return taraxa_rocks_db->update(key, value);
}
std::string SimpleTaraxaRocksDBDelegate::get (const std::string &key) {
  return taraxa_rocks_db->get(key);
}
void SimpleTaraxaRocksDBDelegate::commit() {
// NOP
}

SimpleTaraxaRocksDBDelegate::SimpleTaraxaRocksDBDelegate(const std::string& path):taraxa_rocks_db(std::make_shared<taraxa::RocksDb>(path)){}
