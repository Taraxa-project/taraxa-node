/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-27 12:27:18 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 18:17:26
 */
 
#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include <iostream>
#include <vector>
#include <queue>
#include "types.hpp"
#include "util.hpp"
#include "proto/taraxa_grpc.grpc.pb.h"

namespace taraxa{

using std::string;

class Transaction{
public:
	enum class Type: uint8_t{
		Null,
		Creation,
		Call
	};
	Transaction() = default;
	Transaction(::taraxa_grpc::ProtoTransaction const & t): hash_(t.hash()), type_(toEnum<Type>(t.type())), nonce_(t.nonce()), 
		value_(t.value()), gas_price_(t.gas_price()), gas_(t.gas()), receiver_(t.receiver()), sig_(t.sig()), data_(str2bytes(t.data())){}
	Transaction(trx_hash_t const & hash, Type type, val_t const & nonce, val_t const & value, val_t const & gas_price, val_t const & gas, 
		name_t const & receiver, sig_t const & sig, bytes const &data) try:
		hash_(hash), type_(type), nonce_(nonce), value_(value), gas_price_(gas_price), gas_(gas), receiver_(receiver), sig_(sig), data_(data){
	} catch (std::exception &e){
		std::cerr<<e.what()<<std::endl;
	}
	
	Transaction(stream & strm);
	Transaction(string const & json);
	trx_hash_t getHash() const {return hash_;}
	Type getType() const { return type_;}
	val_t getNonce() const { return nonce_;}
	val_t getValue() const { return value_;}
	val_t getGasPrice() const { return gas_price_;}
	val_t getGas() const { return gas_;}
	name_t getReceiver() const { return receiver_;}
	sig_t getSig() const { return sig_;}
	bytes getData() const {return data_;}

	friend std::ostream & operator<<(std::ostream &strm, Transaction const &trans){
		strm<<"[Transaction] "<< std::endl;
		strm<<"  hash: "<< trans.hash_<<std::endl;
		strm<<"  type: "<< asInteger(trans.type_)<<std::endl;
		strm<<"  nonce: "<< trans.nonce_<<std::endl;
		strm<<"  value: "<< trans.value_<<std::endl;
		strm<<"  gas_price: "<< trans.gas_price_<<std::endl;
		strm<<"  gas: "<<trans.gas_<<std::endl;
		strm<<"  sig: "<<trans.sig_<<std::endl;
		strm<<"  receiver: "<<trans.receiver_<<std::endl;
		strm<<"  data: "<<bytes2str(trans.data_)<<std::endl;
		return strm;
	}
	bool serialize (stream &strm) const;
	bool deserialize (stream &strm);
	string getJsonStr() const;
	bool isValid() const { return !hash_.isZero();}
	bool operator== (Transaction const & other) const {
		return this->getJsonStr() == other.getJsonStr();
	}
protected:
	trx_hash_t hash_ = "0000000000000000000000000000000000000000000000000000000000000000";
	Type type_ = Type::Null;
	val_t nonce_ = "0000000000000000000000000000000000000000000000000000000000000000";
	val_t value_ = "0000000000000000000000000000000000000000000000000000000000000000";
	val_t gas_price_ = "0000000000000000000000000000000000000000000000000000000000000000";
	val_t gas_ = "0000000000000000000000000000000000000000000000000000000000000000";
	name_t receiver_ = "0000000000000000000000000000000000000000000000000000000000000000";
	sig_t sig_ = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
	bytes data_;
};

class TransactionManager{
public:
	void enqueue(Transaction const & trx){ trx_qu_.emplace_back(trx);}
	Transaction front() {return trx_qu_.front();}
	void popFront() {trx_qu_.pop_front();}
private:
	std::deque<Transaction> trx_qu_;
	std::vector<std::thread> process_threads_;
	std::mutex mutex_;

};


} // namespace taraxa


#endif
