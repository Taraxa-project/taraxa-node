/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-27 15:39:04 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-28 14:19:47
 */
 
#include <vector>
#include <thread>
#include <gtest/gtest.h>

#include <grpcpp/server.h>
#include <grpcpp/server_context.h>

#include "transaction.hpp"

namespace taraxa {
	TEST(transaction, transaction){
		unsigned trans; 
		
	}

void runServer() {
  std::string server_address("127.0.0.1:10077");
  TransactionService service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

}

int main(int argc, char* argv[]){
	taraxa::runServer();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
