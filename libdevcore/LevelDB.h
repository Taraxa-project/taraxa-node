// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2017-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include "db.h"

#include <boost/filesystem.hpp>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include "Log.h"
#include <boost/asio.hpp>
#include <thread>


namespace dev
{
namespace db
{
class LevelDB : public DatabaseFace
{
public:
    static leveldb::ReadOptions defaultReadOptions();
    static leveldb::WriteOptions defaultWriteOptions();
    static leveldb::Options defaultDBOptions();

    explicit LevelDB(boost::filesystem::path const& _path,
        leveldb::ReadOptions _readOptions = defaultReadOptions(),
        leveldb::WriteOptions _writeOptions = defaultWriteOptions(),
        leveldb::Options _dbOptions = defaultDBOptions());
    virtual ~LevelDB() {
      if(perf_) {
        timer_.cancel();
        io_service_.stop();
        thread_.join();
      }
    }

    std::string lookup(Slice _key) const override;
    bool exists(Slice _key) const override;
    void insert(Slice _key, Slice _value) override;
    void kill(Slice _key) override;

    std::unique_ptr<WriteBatchFace> createWriteBatch() const override;
    void commit(std::unique_ptr<WriteBatchFace> _batch) override;

    void forEach(std::function<bool(Slice, Slice)> _f) const override;

    static void setPerf(bool perf) { perf_ = perf; }

private:
    std::unique_ptr<leveldb::DB> m_db;
    leveldb::ReadOptions const m_readOptions;
    leveldb::WriteOptions const m_writeOptions;
    std::string path_;

    //Temporary to measure performance, remove later
    inline static bool perf_ = false;
    mutable uint64_t perf_read_time_ = 0;
    mutable uint64_t perf_read_count_ = 0;
    mutable uint64_t perf_write_time_ = 0;
    mutable uint64_t perf_write_count_ = 0;
    mutable uint64_t perf_delete_time_ = 0;
    mutable uint64_t perf_delete_count_ = 0;
    boost::asio::io_service io_service_;
    boost::asio::deadline_timer timer_;
    std::thread thread_;
    void writePerformanceLog();
    dev::Logger log_perf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "DBPER")};

};

}  // namespace db
}  // namespace dev
