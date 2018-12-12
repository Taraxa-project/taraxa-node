/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 11:19:05 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 14:01:23
 */

#include "rocks_db.hpp"

namespace taraxa{
using namespace rocksdb;

RocksDb::RocksDb(std::string path):db_path_(path){
	opt_.IncreaseParallelism();
	opt_.OptimizeLevelStyleCompaction();
	opt_.create_if_missing = true;
	rocksdb::Status status = rocksdb::DB::Open(opt_, db_path_, &db_);
	if (!status.ok()){
		std::cerr<<"Cannot open data base " << db_path_<< "\n";
		throw std::invalid_argument("Open DB fail \n");
	}
}
RocksDb::~RocksDb(){
	delete db_;
}

bool RocksDb::put(const std::string &key, const std::string &value){
	if (!db_) return false;
	rocksdb::Status s = db_->Put(WriteOptions(), key, value);
	assert(s.ok());
	return true;
}

std::string RocksDb::get(const std::string &key){
	PinnableSlice pinnable_val;
	rocksdb::Status s= db_->Get(ReadOptions(), db_->DefaultColumnFamily(), key, &pinnable_val);
	assert(s.ok());
	std::string ret = pinnable_val.ToString();
	pinnable_val.Reset();
	return ret;
}

bool RocksDb::erase(const std::string &key){
	rocksdb::Status s = db_->Delete(WriteOptions(), key);
	assert(s.ok());
	return true;
}
} // namespace taraxa
