/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-15 16:32:16 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 18:19:00
 */

#include "grpc_server.hpp"

namespace taraxa{

::grpc::Status GrpcService::SendProtoTransaction(::grpc::ServerContext* context, const ::taraxa_grpc::ProtoTransaction* request, ::taraxa_grpc::SendProtoTransactionResponse* response){
	Transaction trans(*request);
	std::string hash = trans.getHash().toString();
	transactions_[hash] = trans.getJsonStr();
	response->set_success(1);
	return ::grpc::Status::OK;
}
::grpc::Status GrpcService::GetProtoTransaction(::grpc::ServerContext* context, const ::taraxa_grpc::GetProtoTransactionRequest* request, ::taraxa_grpc::ProtoTransaction* response){
	auto iter = transactions_.find(request->hash());
	Transaction trans;
	if (iter != transactions_.end()){
		trans= iter->second;
	}   
	else {
		std::cout<<"Transaction not exist ..."<<std::endl;
	}
	setProtoTransaction(trans, response);
	return ::grpc::Status::OK;

}
void GrpcService::start(){
	std::string server_address("0.0.0.0:10077");
  	builder_.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  	builder_.RegisterService(this);
	server_ = builder_.BuildAndStart();
  	server_->Wait();
}

void GrpcService::stop(){
	server_.reset();
}
 
} // namespace taraxa

