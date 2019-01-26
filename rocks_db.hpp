/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 11:45:41 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:22:26
 */
  
#ifndef ROCKS_DB_HPP
#define ROCKS_DB_HPP

#include <iostream>
#include <string>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <memory>
#include <utility>
#include <mutex>


namespace taraxa{
class RocksDb{
public:

	RocksDb(std::string path);
	~RocksDb();
	bool put(const std::string &key, const std::string &value);
	std::string get(const std::string &key);
	bool erase (const std::string &key);
	void setVerbose(bool verbose);
	
private:
	bool verbose_ = false;
	std::string db_path_;
	rocksdb::DB *db_;
	rocksdb::Options opt_;
	std::mutex mutex_;
};
} // namespace taraxa

#endif
