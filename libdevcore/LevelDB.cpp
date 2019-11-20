/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "LevelDB.h"
#include <boost/bind.hpp>
#include "Assertions.h"

namespace dev {
namespace db {
namespace {
inline leveldb::Slice toLDBSlice(Slice _slice) {
  return leveldb::Slice(_slice.data(), _slice.size());
}

DatabaseStatus toDatabaseStatus(leveldb::Status const& _status) {
  if (_status.ok())
    return DatabaseStatus::Ok;
  else if (_status.IsIOError())
    return DatabaseStatus::IOError;
  else if (_status.IsCorruption())
    return DatabaseStatus::Corruption;
  else if (_status.IsNotFound())
    return DatabaseStatus::NotFound;
  else
    return DatabaseStatus::Unknown;
}

void checkStatus(leveldb::Status const& _status,
                 boost::filesystem::path const& _path = {}) {
  if (_status.ok()) return;

  DatabaseError ex;
  ex << errinfo_dbStatusCode(toDatabaseStatus(_status))
     << errinfo_dbStatusString(_status.ToString());
  if (!_path.empty()) ex << errinfo_path(_path.string());

  BOOST_THROW_EXCEPTION(ex);
}

class LevelDBWriteBatch : public WriteBatchFace {
 public:
  void insert(Slice _key, Slice _value) override;
  void kill(Slice _key) override;

  leveldb::WriteBatch const& writeBatch() const { return m_writeBatch; }
  leveldb::WriteBatch& writeBatch() { return m_writeBatch; }

 private:
  leveldb::WriteBatch m_writeBatch;
};

void LevelDBWriteBatch::insert(Slice _key, Slice _value) {
  m_writeBatch.Put(toLDBSlice(_key), toLDBSlice(_value));
}

void LevelDBWriteBatch::kill(Slice _key) {
  m_writeBatch.Delete(toLDBSlice(_key));
}

}  // namespace

leveldb::ReadOptions LevelDB::defaultReadOptions() {
  return leveldb::ReadOptions();
}

leveldb::WriteOptions LevelDB::defaultWriteOptions() {
  return leveldb::WriteOptions();
}

leveldb::Options LevelDB::defaultDBOptions() {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 256;
  return options;
}

void LevelDB::writePerformanceLog() {
  if (io_service_.stopped()) return;
  LOG(log_perf_)
      << path_ << std::endl
      << "Read count: " << perf_read_count_ << " - Avg time: "
      << perf_read_time_ / ((perf_read_count_ > 0) ? perf_read_count_ : 1)
      << "[µs]" << std::endl
      << "Write count: " << perf_write_count_ << " - Avg time: "
      << perf_write_time_ / ((perf_write_count_ > 0) ? perf_write_count_ : 1)
      << "[µs]" << std::endl
      << "Erase count: " << perf_delete_count_ << " - Avg time: "
      << perf_delete_time_ / ((perf_delete_count_ > 0) ? perf_delete_count_ : 1)
      << "[µs]" << std::endl;
  timer_.expires_from_now(boost::posix_time::seconds(20));
  timer_.async_wait(boost::bind(&LevelDB::writePerformanceLog, this));
}

LevelDB::LevelDB(boost::filesystem::path const& _path,
                 leveldb::ReadOptions _readOptions,
                 leveldb::WriteOptions _writeOptions,
                 leveldb::Options _dbOptions)
    : m_db(nullptr),
      m_readOptions(std::move(_readOptions)),
      m_writeOptions(std::move(_writeOptions)),
      path_(_path.c_str()),
      io_service_(),
      timer_(io_service_) {
  if (perf_) {
    thread_ = std::thread([this]() { io_service_.run(); });
    timer_.expires_from_now(boost::posix_time::seconds(20));
    timer_.async_wait(boost::bind(&LevelDB::writePerformanceLog, this));
  }
  auto db = static_cast<leveldb::DB*>(nullptr);
  auto const status = leveldb::DB::Open(_dbOptions, _path.string(), &db);
  checkStatus(status, _path);

  assert(db);
  m_db.reset(db);
}

std::string LevelDB::lookup(Slice _key) const {
  std::chrono::steady_clock::time_point begin;
  if (perf_) {
    begin = std::chrono::steady_clock::now();
  }
  leveldb::Slice const key(_key.data(), _key.size());
  std::string value;
  auto const status = m_db->Get(m_readOptions, key, &value);
  if (perf_) {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    perf_read_count_++;
    perf_read_time_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
  }
  if (status.IsNotFound()) return std::string();

  checkStatus(status);
  return value;
}

bool LevelDB::exists(Slice _key) const {
  std::chrono::steady_clock::time_point begin;
  if (perf_) {
    begin = std::chrono::steady_clock::now();
  }
  std::string value;
  leveldb::Slice const key(_key.data(), _key.size());
  auto const status = m_db->Get(m_readOptions, key, &value);
  if (perf_) {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    perf_read_count_++;
    perf_read_time_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
  }
  if (status.IsNotFound()) return false;

  checkStatus(status);
  return true;
}

void LevelDB::insert(Slice _key, Slice _value) {
  std::chrono::steady_clock::time_point begin;
  if (perf_) {
    begin = std::chrono::steady_clock::now();
  }
  leveldb::Slice const key(_key.data(), _key.size());
  leveldb::Slice const value(_value.data(), _value.size());
  auto const status = m_db->Put(m_writeOptions, key, value);
  checkStatus(status);
  if (perf_) {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    perf_write_count_++;
    perf_write_time_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
  }
}

void LevelDB::kill(Slice _key) {
  std::chrono::steady_clock::time_point begin;
  if (perf_) {
    begin = std::chrono::steady_clock::now();
  }
  leveldb::Slice const key(_key.data(), _key.size());
  auto const status = m_db->Delete(m_writeOptions, key);
  checkStatus(status);
  if (perf_) {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    perf_delete_count_++;
    perf_delete_time_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
  }
}

std::unique_ptr<WriteBatchFace> LevelDB::createWriteBatch() const {
  return std::unique_ptr<WriteBatchFace>(new LevelDBWriteBatch());
}

void LevelDB::commit(std::unique_ptr<WriteBatchFace> _batch) {
  std::chrono::steady_clock::time_point begin;
  if (perf_) {
    begin = std::chrono::steady_clock::now();
  }
  if (!_batch) {
    BOOST_THROW_EXCEPTION(DatabaseError()
                          << errinfo_comment("Cannot commit null batch"));
  }
  auto* batchPtr = dynamic_cast<LevelDBWriteBatch*>(_batch.get());
  if (!batchPtr) {
    BOOST_THROW_EXCEPTION(DatabaseError() << errinfo_comment(
                              "Invalid batch type passed to LevelDB::commit"));
  }
  auto const status = m_db->Write(m_writeOptions, &batchPtr->writeBatch());
  checkStatus(status);
  if (perf_) {
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    perf_write_count_++;
    perf_write_time_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
  }
}

void LevelDB::forEach(std::function<bool(Slice, Slice)> _f) const {
  std::unique_ptr<leveldb::Iterator> itr(m_db->NewIterator(m_readOptions));
  if (itr == nullptr) {
    BOOST_THROW_EXCEPTION(DatabaseError() << errinfo_comment("null iterator"));
  }
  auto keepIterating = true;
  for (itr->SeekToFirst(); keepIterating && itr->Valid(); itr->Next()) {
    auto const dbKey = itr->key();
    auto const dbValue = itr->value();
    Slice const key(dbKey.data(), dbKey.size());
    Slice const value(dbValue.data(), dbValue.size());
    keepIterating = _f(key, value);
  }
}

}  // namespace db
}  // namespace dev
