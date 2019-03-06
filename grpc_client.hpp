/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-03-05 16:11:46 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 18:01:40
 */
 
#ifndef GRPC_CLIENT_HPP
#define GRPC_CLIENT_HPP
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "transaction.hpp"
#include "proto/taraxa_grpc.grpc.pb.h"
#include "grpc_util.hpp"

namespace taraxa{
class GrpcClient {
public:
	GrpcClient(std::shared_ptr<::grpc::Channel> channel);
	void sendTransaction(Transaction const & trx);
	Transaction getTransaction(trx_hash_t hash);
private:
	// ::grpc::ClientContext context_;
	std::unique_ptr<::taraxa_grpc::TaraxaProtoService::Stub> stub_;
	std::vector<Transaction> transcations_;
};
} // namespace taraxa


#endif
