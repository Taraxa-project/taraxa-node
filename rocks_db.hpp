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


namespace taraxa{
class RocksDb{
public:

	RocksDb(std::string path):db_path_(path){
		opt_.IncreaseParallelism();
		opt_.OptimizeLevelStyleCompaction();
		opt_.create_if_missing = true;
		rocksdb::Status status = rocksdb::DB::Open(opt_, db_path_, &db_);
		if (!status.ok()){
			std::cerr<<"Cannot open data base " << db_path_<< "\n";
			throw std::invalid_argument("Open DB fail \n");
		}
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
} // namespace taraxa

#endif
