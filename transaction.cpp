#include "transaction.hpp"
#include <grpcpp/server_builder.h>
#include <string>

namespace taraxa{
std::string str(uint256_t num){
	std::stringstream ss;
	ss<<num;
	return ss.str();
	
}
void setGrpcTransaction(::taraxa_ledger::GrpcTransaction* ret, Transaction const & t){
	ret->set_nonce(str(t.getNonce()));
	ret->set_value(str(t.getValue()));
	// ret->set_gas_price(t.getGasPrice());
	// ret->set_gas(t.getGas());
	// ret->set_receiver(t.getReceiver());
	// //ret->set_signature(t.getSig());
	// ret->set_data(t.set_data());
}
 
using namespace taraxa_ledger;
TransactionService::TransactionService(){}
::grpc::Status TransactionService::SendGrpcTransaction(::grpc::ServerContext* context, const ::taraxa_ledger::GrpcTransaction* request, ::taraxa_ledger::SendGrpcTransactionResponse* response){
	std::cout<<"Server: SendGrpcTransaction request ..."<<std::endl;
	transactions_.push_back(*request);
	return Status::OK;
}

::grpc::Status TransactionService::GetGrpcTransaction(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::taraxa_ledger::GrpcTransaction* response){
	if (!transactions_.empty()){
		std::cout<<"Server: GetGrpcTransaction request ..."<<std::endl;
		setGrpcTransaction(response, transactions_.back());
	}
	return Status::OK;

}
}