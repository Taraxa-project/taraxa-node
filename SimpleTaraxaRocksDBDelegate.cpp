//
// Created by JC on 2019-04-26.
//
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

SimpleTaraxaRocksDBDelegate::SimpleTaraxaRocksDBDelegate(const std::string& path):
  taraxa_rocks_db(std::make_shared<taraxa::RocksDb>(path)){}
