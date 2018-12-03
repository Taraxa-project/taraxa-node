/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 11:45:41 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 14:17:21
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

// Two DB
// 1. block db: hash of block -> state block
// 2. account db: pk -> account state

class RocksDb{
public:

	RocksDb(std::string path):db_path_(path){
		opt_.IncreaseParallelism();
		opt_.OptimizeLevelStyleCompaction();
		opt_.create_if_missing = true;
		rocksdb::Status s = rocksdb::DB::Open(opt_, db_path_, &db_);
		assert(s.ok());
	}
	~RocksDb(){
		delete db_;
	}
	bool put(const std::string &key, const std::string &value);
	std::string get(const std::string &key);
	bool erase (const std::string &key);
	
private:
	std::string db_path_;
	rocksdb::DB *db_;
	rocksdb::Options opt_;
};
#endif
