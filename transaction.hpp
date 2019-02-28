/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-27 12:27:18 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-27 19:13:34
 */
 
#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include <iostream>
#include "types.hpp"
#include "util.hpp"

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>

#include "proto/transaction.grpc.pb.h"

namespace taraxa{

class Transaction{
public:
	enum class Type{
		Null,
		Creation,
		Call
	};
	friend std::ostream & operator<<(std::ostream &strm, Transaction const &trans){
		strm<<"[Transaction] "<< asInteger(trans.type_)<<std::endl;
		strm<<"  nonce: "<< trans.nonce_<<std::endl;
		strm<<"  value: "<< trans.value_<<std::endl;
		strm<<"  gas_price: "<< trans.gas_price_<<std::endl;
		strm<<"  gas: "<<trans.gas_<<std::endl;
		strm<<"  sig: "<<trans.sig_<<std::endl;
		strm<<"  receiver: "<<trans.receiver_<<std::endl;
		strm<<"  data: "<<trans.data_<<std::endl;
		return strm;
	}
protected:
	Type type_ = Type::Null;
	uint256_t nonce_;
	uint256_t value_;
	uint256_t gas_price_;
	uint256_t gas_;
	name_t    receiver_;
	uint512_t sig_;
	bytes data_;
};

} // namespace taraxa


#endif
