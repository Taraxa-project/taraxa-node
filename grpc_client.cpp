/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-28 18:07:53 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-28 20:44:28
 */
 
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "transaction.hpp"
#include "proto/transaction.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
namespace taraxa{
using namespace taraxa_ledger;
class TransactionClient {
public:
	TransactionClient(std::shared_ptr<Channel> channel): stub_(TaraxaLedgerGrpcService::NewStub(channel)){}
	void sendTransaction(std::string value){
		ClientContext context;
		GrpcTransaction gtrx;
		gtrx.set_value(value);
		SendGrpcTransactionResponse resp;
		Status status = stub_->SendGrpcTransaction(&context, gtrx, &resp);
		
	}
	void getTransaction(){
		ClientContext context;
		GrpcTransaction gtrx;
		::google::protobuf::Empty empty_request;
		Status status = stub_->GetGrpcTransaction(&context, empty_request, &gtrx);
		std::string v = gtrx.value();
		std::cout<<"value = "<< v<<std::endl;
	}
private:
	std::unique_ptr<TaraxaLedgerGrpcService::Stub> stub_;
	std::vector<Transaction> transcations_;
};
} // namespace taraxa

int main(int argc, char* argv[]){
	taraxa::TransactionClient client(CreateChannel("localhost:10077",grpc::InsecureChannelCredentials()));
	client.sendTransaction("1");
	client.getTransaction();
	client.sendTransaction("2");
	client.getTransaction();

	return 1;
}
