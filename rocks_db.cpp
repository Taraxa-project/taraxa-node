/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-10-31 11:19:05
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-29 13:11:59
 */

#include "rocks_db.hpp"
#include <boost/filesystem.hpp>

namespace taraxa {
using namespace rocksdb;

RocksDb::RocksDb(std::string path_str) : db_path_(path_str) {
  boost::filesystem::path path(db_path_);
  if (path.size() == 0) {
    throw std::invalid_argument("Error, invalid db path: " + db_path_);
  }
  if (!boost::filesystem::exists(path)) {
    std::cout << "Create db directory: " << path << std::endl;
    if (!boost::filesystem::create_directories(path)) {
      throw std::invalid_argument("Error, cannot create db path: " + db_path_);
    }
  }
  if (!boost::filesystem::is_directory(path)) {
    throw std::invalid_argument("Error, db path is not directory: " + db_path_);
  }
  rocksdb::Status status;

  // Will remove all old data!
  status = DestroyDB(db_path_, opt_);
  if (status.ok()) {
    std::cout << "Warning! DB is cleared : " << db_path_ << std::endl;
  } else {
    std::cout << "Cannot clear DB : " << db_path_ << std::endl;
    std::cout << status.ToString() << std::endl;
  }

  opt_.IncreaseParallelism();
  opt_.OptimizeLevelStyleCompaction();
  opt_.create_if_missing = true;
  status = rocksdb::DB::Open(opt_, db_path_, &db_);
  if (status.ok()) {
    std::cout << "Open DB: " << db_path_ << " ok " << std::endl;
  } else {
    auto pid = ::getpid();
    std::string new_db_path = db_path_ + std::to_string(pid);
    std::cout << "Open DB fail: " << db_path_ << " because "
              << status.ToString() << std::endl;
    std::cout << "Warning! Create temp DB: " << new_db_path << std::endl;
  }
}
RocksDb::~RocksDb() { delete db_; }

void RocksDb::setVerbose(bool verbose) { verbose_ = verbose; }

bool RocksDb::put(const std::string &key, const std::string &value) {
  std::unique_lock<std::mutex> lock(mutex_);

  std::string str;
  rocksdb::Status s =
      db_->Get(rocksdb::ReadOptions(), db_->DefaultColumnFamily(), key, &str);
  bool seen = s.ok();

  if (seen) {
    if (verbose_) {
      std::cout << "Warning! Data exist, returnd. Key = " << key << std::endl;
    }
    return false;
  }

  s = db_->Put(WriteOptions(), key, value);
  if (!s.ok()) {
    std::cout << s.ToString() << std::endl;
  }
  assert(s.ok());

  return true;
}

std::string RocksDb::get(const std::string &key) {
  std::unique_lock<std::mutex> lock(mutex_);
  PinnableSlice pinnable_val;
  rocksdb::Status s =
      db_->Get(ReadOptions(), db_->DefaultColumnFamily(), key, &pinnable_val);
  bool ok = s.ok() || s.IsNotFound();

  if (!ok) {
    std::cout << s.ToString() << std::endl;
  }
  assert(ok);
  std::string ret = pinnable_val.ToString();
  pinnable_val.Reset();
  return ret;
}

bool RocksDb::erase(const std::string &key) {
  std::unique_lock<std::mutex> lock(mutex_);
  rocksdb::Status s = db_->Delete(WriteOptions(), key);
  assert(s.ok());
  return true;
}
}  // namespace taraxa
