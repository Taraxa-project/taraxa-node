/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-28 18:07:53 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 18:19:46
 */
 
#include "grpc_client.hpp"
#include "transaction.hpp"

namespace taraxa{

GrpcClient::GrpcClient(std::shared_ptr<::grpc::Channel> channel): 
	stub_(::taraxa_grpc::TaraxaProtoService::NewStub(channel)){}

void GrpcClient::sendTransaction(Transaction const & trx){
	::grpc::ClientContext context;

	::taraxa_grpc::ProtoTransaction ptrx;
	setProtoTransaction(trx, &ptrx);
	::taraxa_grpc::SendProtoTransactionResponse response;
	::grpc::Status status = stub_->SendProtoTransaction(&context, ptrx, &response);
	
}
Transaction GrpcClient::getTransaction(trx_hash_t hash){
	::grpc::ClientContext context;

	::taraxa_grpc::ProtoTransaction ptrx;
	::taraxa_grpc::GetProtoTransactionRequest request;
	request.set_hash(hash.toString());
	::grpc::Status status = stub_->GetProtoTransaction(&context, request, &ptrx);
	Transaction trx(ptrx);
	return trx;
}

} // namespace taraxa
