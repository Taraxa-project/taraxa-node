/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-15 16:32:16 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-20 18:06:58
 */
 
#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "./grpc/proto/ledger.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using ledger::LedgerService;

class NodeRpcServer final : public LedgerService::Service{

	Status IsTestnet(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::IsTestnetResponse* response){
		response->set_is_testnet(true);
		return Status::OK;
	}
	Status GetNetworkName(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetNetworkNameResponse* response){
		response->set_network_name("taraxa.io");
	}
	Status GetVersion(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetVersionResponse* response){
		response->set_version("0.0.1");
	}
	Status GetSubversion(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetSubversionResponse* response){
		response->set_sub_version(".1");
	}
	Status GetCoinName(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetCoinNameResponse* response){
		response->set_coin_name("taraxa.coin");
	}
	Status GetChainInfo(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetChainInfoResponse* response){
		ledger::ChainInfo info;
		info.set_chain_name("ChainInfo: taraxa");
		response->set_allocated_chain_info(&info);
	}
	Status GetBestBlockHash(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetBestBlockHashResponse* response){}
	Status GetBestBlockHeight(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ledger::GetBestBlockHeightResponse* response){}
	Status GetBlockHash(::grpc::ServerContext* context, const ::ledger::GetBlockHashRequest* request, ::ledger::GetBlockHashResponse* response){}
	Status GetBlockHeader(::grpc::ServerContext* context, const ::ledger::GetBlockHeaderRequest* request, ::ledger::GetBlockHeaderResponse* response){}
	Status GetBlock(::grpc::ServerContext* context, const ::ledger::GetBlockRequest* request, ::ledger::GetBlockResponse* response){}
	Status GetBlockInfo(::grpc::ServerContext* context, const ::ledger::GetBlockInfoRequest* request, ::ledger::GetBlockInfoResponse* response){}
};

int main(int argc, char *argv[]){
	return 0;
}
void RunServer(const std::string& db_path) {
  std::string server_address("0.0.0.0:50051");
  NodeRpcServer service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}
