/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-03-05 15:30:41 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 17:43:47
 */

#ifndef GRPC_SERVER_HPP
#define GRPC_SERVER_HPP

#include <map>
#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "proto/taraxa_grpc.grpc.pb.h"
#include "transaction.hpp"
#include "grpc_util.hpp"

namespace taraxa{


class GrpcService final : public taraxa_grpc::TaraxaProtoService::Service{
public:
	GrpcService(){}
	~GrpcService() override {}
	void start();
	void stop();
	::grpc::Status SendProtoTransaction(::grpc::ServerContext* context, const ::taraxa_grpc::ProtoTransaction* request, ::taraxa_grpc::SendProtoTransactionResponse* response) override;
    ::grpc::Status GetProtoTransaction(::grpc::ServerContext* context, const ::taraxa_grpc::GetProtoTransactionRequest* request, ::taraxa_grpc::ProtoTransaction* response) override;
private:
	std::map<std::string, std::string> transactions_;
	::grpc::ServerBuilder builder_;
	std::unique_ptr<::grpc::Server> server_ = nullptr;

};

// void runGrpcServer(GrpcService &service);

}


#endif
