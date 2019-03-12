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

TransactionQueue::TransactionQueue(unsigned current_capacity, unsigned future_capacity): 
current_capacity_(current_capacity),
future_capacity_(future_capacity){
	
}

void TransactionQueue::verifyTrx(){
	while (!stopped_){
		UnverifiedTrx utrx;
		ulock lock(mutex_for_unverified_qu_);
		while (unverified_qu_.empty() && !stopped_){
			cond_for_unverified_qu_.wait(lock);
		}
		if (stopped_) return;
	
		utrx = std::move(unverified_qu_.front());
		unverified_qu_.pop_front();

		try {
			Transaction trx = std::move(utrx.trx);
			node_id_t node_id = std::move(utrx.node_id);
			// verify and put the transaction to verified queue
		} catch (...) {

		}

	}	
}

}// namespace taraxa