#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <grpcpp/server_builder.h>
#include <string>
#include <utility>
#include "transaction.hpp"

namespace taraxa{

using namespace taraxa_grpc;



Transaction::Transaction(stream &strm){
	deserialize(strm);
}

Transaction::Transaction(string const &json){
	try{
		boost::property_tree::ptree doc = strToJson(json);
		hash_ = doc.get<string>("hash");
		type_ = toEnum<Transaction::Type>(doc.get<uint8_t>("type"));
		nonce_ = doc.get<string>("nonce");
		value_ = doc.get<string>("value");
		gas_price_ = doc.get<string>("gas_price");
		gas_ = doc.get<string>("gas");
		sig_ = doc.get<string>("sig");
		receiver_ = doc.get<string>("receiver");
		string data = doc.get<string>("data");
		data_ = str2bytes(data);
	} catch (std::exception &e){
		std::cerr<<e.what()<<std::endl;
	}
}

bool Transaction::serialize(stream & strm) const{
	bool ok = true;
	ok &= write(strm, hash_);
	ok &= write(strm, type_);
	ok &= write(strm, nonce_);
	ok &= write(strm, value_);
	ok &= write(strm, gas_price_);
	ok &= write(strm, gas_);
	ok &= write(strm, receiver_);
	ok &= write(strm, sig_);
	std::size_t byte_size = data_.size();
	ok &= write(strm, byte_size);
	for (auto i=0; i<byte_size; ++i){
		ok &= write(strm, data_[i]);
	}
	assert(ok);
	return ok;
}

bool Transaction::deserialize(stream &strm){
	bool ok = true;
	ok &= read(strm, hash_);
	ok &= read(strm, type_);
	ok &= read(strm, nonce_);
	ok &= read(strm, value_);
	ok &= read(strm, gas_price_);
	ok &= read(strm, gas_);
	ok &= read(strm, receiver_);
	ok &= read(strm, sig_);
	std::size_t byte_size;
	ok &= read(strm, byte_size);
	data_.resize(byte_size);
	for (auto i=0; i<byte_size; ++i){
		ok &= read(strm, data_[i]);
	}
	assert(ok);
	return ok;
}
string Transaction::getJsonStr() const {

	boost::property_tree::ptree tree;
	tree.put("hash", hash_.toString());
	tree.put("type", asInteger(type_));
	tree.put("nonce", nonce_.toString());
	tree.put("value", value_.toString());
	tree.put("gas_price", gas_price_.toString());
	tree.put("gas", gas_.toString());
	tree.put("sig", sig_.toString());
	tree.put("receiver", receiver_.toString());
	tree.put("data", bytes2str(data_));
	std::stringstream ostrm;
	boost::property_tree::write_json(ostrm, tree);
	return ostrm.str();
}
bool TransactionQueue::insert(Transaction trx){
	trx_hash_t hash = trx.getHash();
	auto status = trx_status_.get(hash);
	bool ret; 
	if (status.second == false){ // never seen before
		ret = trx_status_.insert(hash, TransactionStatus::seen_in_queue);
		uLock lock(mutex_for_unverified_qu_);
		unverified_qu_.emplace_back(trx);
		LOG(logger_)<<"Trx: "<< hash << "inserted. "<<std::endl;
	} else if (status.first == TransactionStatus::unseen_but_already_packed_by_others){ // updated by other blocks
		ret = trx_status_.compareAndSwap(hash, 
			TransactionStatus::unseen_but_already_packed_by_others, 
			TransactionStatus::seen_in_queue_but_already_packed_by_others);
		uLock lock(mutex_for_unverified_qu_);
		unverified_qu_.emplace_back(trx);
		LOG(logger_)<<"Trx: "<< hash << "already packed by others, but still enqueue. "<<std::endl;
	} 
	return ret;
}
void TransactionQueue::verifyTrx(){
	while (!stopped_){
		Transaction utrx;
		uLock lock(mutex_for_unverified_qu_);
		while (unverified_qu_.empty() && !stopped_){
			cond_for_unverified_qu_.wait(lock);
		}
		if (stopped_) return;
	
		utrx = std::move(unverified_qu_.front());
		unverified_qu_.pop_front();

		try {
			Transaction trx = std::move(utrx);
			// verify and put the transaction to verified queue
			bool valid = true;
			// mark invalid
			if (!valid){
				trx_status_.compareAndSwap(trx.getHash(), TransactionStatus::seen_in_queue, TransactionStatus::seen_but_invalid);
			} else{
				// push to verified qu
				uLock lock(mutex_for_verified_qu_);
				verified_trxs_[trx.getHash()] = trx;
			}
		} catch (...) {

		}

	}	
}

bool TransactionManager::insertTrx(Transaction trx){
	trx_qu_.insert(trx);
	return true;
}
bool TransactionManager::setPackedTrxFromBlock(blk_hash_t blk){
	
}
/**
 * This is for block proposer 
 * Few steps:
 * 1. get a snapshot (move) of verified queue (lock)
 *	  now, verified trxs can include (A) unpacked ,(B) packed by other ,(C) old trx that only seen in db
 * 2. write A, B to database, of course C will be rejected (already exist in database)
 * 3. propose transactions for block A
 * 4. update A, B and C status to seen_in_db 
 * 
 */ 
bool TransactionManager::packTrxs(std::vector<trx_hash_t> & to_be_packed_trx){
	auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot();
	std::vector<trx_hash_t> exist_in_db;
	std::vector<trx_hash_t> packed_by_others;
	to_be_packed_trx.clear();
	uLock lock(mutex_);	
	for (auto const & i: verified_trx){
		trx_hash_t const & hash = i.first;
		Transaction const & trx = i.second;
		if (!db_trxs_->put(hash.toString(), trx.getJsonStr())){
			exist_in_db.emplace_back(i.first);
		}
		TransactionStatus status; 
		bool exist; 
		std::tie(status, exist) = trx_status_.get(hash);
		assert(exist);
		if (status == TransactionStatus::seen_in_queue_but_already_packed_by_others){
			packed_by_others.emplace_back(hash);
		} else if (status == TransactionStatus::seen_in_queue){
			to_be_packed_trx.emplace_back(i.first);
		} else{
			LOG(logger_)<<"Warning! Trx: "<<hash<<" status "<<asInteger(status)<<std::endl;
			assert(true);
		}
	}
	// update transaction_status
	for (auto const & t: exist_in_db){
		trx_status_.insert(t, TransactionStatus::seen_in_db);
	}
	for (auto const & t: packed_by_others){
		trx_status_.insert(t, TransactionStatus::seen_in_db);
	}
	for (auto const & t: to_be_packed_trx){
		trx_status_.insert(t, TransactionStatus::seen_in_db);
	}
	return true;
}


}// namespace taraxa