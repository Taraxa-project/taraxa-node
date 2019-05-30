/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-10-31 11:45:41
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:22:26
 */

#ifndef ROCKS_DB_HPP
#define ROCKS_DB_HPP

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include "libdevcore/Log.h"

namespace taraxa {
class RocksDb : public std::enable_shared_from_this<RocksDb> {
 public:
  RocksDb(std::string path, bool overwrite);
  ~RocksDb();
  bool put(const std::string &key, const std::string &value);
  bool update(const std::string &key, const std::string &value);
  std::string get(const std::string &key);
  bool erase(const std::string &key);
  void setVerbose(bool verbose);
  std::shared_ptr<RocksDb> getShared() {
    try {
      return shared_from_this();
    } catch (std::bad_weak_ptr &e){
      std::cerr<<"RocksDb: "<<e.what()<<std::endl;
      return nullptr;
    }
  }
 private:
  bool verbose_ = false;
  std::string db_path_;
  rocksdb::DB *db_;
  rocksdb::Options opt_;
  std::mutex mutex_;
  mutable dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "ROCKDB")};
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "ROCKDB")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "ROCKDB")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "ROCKDB")};
};
}  // namespace taraxa

#endif
