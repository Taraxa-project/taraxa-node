/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-04-27 15:44:15
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 15:44:38
 */

#ifndef TARAXA_NODE_SIMPLETARAXAROCKSDBDELEGATE_H
#define TARAXA_NODE_SIMPLETARAXAROCKSDBDELEGATE_H
#include "SimpleDBFace.h"
#include "rocks_db.hpp"

class SimpleTaraxaRocksDBDelegate : public SimpleDBFace {
 public:
  bool put(const std::string &key, const std::string &value) override;
  bool update(const std::string &key, const std::string &value) override;
  std::string get(const std::string &key) override;
  void commit() override;
  SimpleTaraxaRocksDBDelegate(const std::string &path, bool overwrite);

  bool put(const h256 &key, const dev::bytes &value) { assert(false); }
  bool update(const h256 &key, const dev::bytes &value) { assert(false); }
  dev::bytes get(const h256 &key) { assert(false); }

 private:
  std::shared_ptr<taraxa::RocksDb> taraxa_rocks_db = nullptr;
};

#endif  // TARAXA_NODE_SIMPLETARAXAROCKSDBDELEGATE_H
