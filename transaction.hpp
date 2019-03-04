/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-27 12:27:18 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-28 20:39:46
 */
 
#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include <iostream>
#include <vector>
#include "types.hpp"
#include "util.hpp"

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>

#include "proto/transaction.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

namespace taraxa{

using namespace taraxa_ledger;

class Transaction{
public:
	enum class Type{
		Null,
		Creation,
		Call
	};
	Transaction():
		type_(Type::Null), nonce_(1), value_(2), gas_price_(3), gas_(4), receiver_("me"), sig_(6){}
	Transaction(::taraxa_ledger::GrpcTransaction const & t): nonce_(t.nonce()), value_(t.value()), gas_price_(t.gas_price()),
		gas_(t.gas()), receiver_(t.receiver()), sig_(t.receiver()){}
	Type getType() const { return type_;}
	uint256_t getNonce() const { return nonce_;}
	uint256_t getValue() const { return value_;}
	uint256_t getGasPrice() const { return gas_price_;}
	uint256_t getGas() const { return gas_;}
	std::string getReceiver() const { return receiver_;}
	uint512_t getSig() const { return sig_;}
	bytes & getBytes() {return data_;}
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
	std::string    receiver_;
	uint512_t sig_;
	bytes data_;
};

class TransactionService final : public taraxa_ledger::TaraxaLedgerGrpcService::Service{
public:
	TransactionService();
	::grpc::Status SendGrpcTransaction(::grpc::ServerContext* context, const ::taraxa_ledger::GrpcTransaction* request, ::taraxa_ledger::SendGrpcTransactionResponse* response) override;
	::grpc::Status GetGrpcTransaction(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::taraxa_ledger::GrpcTransaction* response) override;


private:
	std::vector<Transaction> transactions_;
	ServerBuilder builder_;

};

} // namespace taraxa


#endif
