#include "jsonrpc_server.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block.hpp"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "network/graphql/query.hpp"
#include "transaction_manager/transaction.hpp"
#include "util/util.hpp"

namespace taraxa::net {

std::shared_ptr<HttpConnection> JsonRpcServer::createConnection() {
  return std::make_shared<JsonRpcConnection>(getShared());
}

std::string JsonRpcConnection::processRequest(std::string request) {
  string response;
  if (http_->GetHandler() != NULL) {
    http_->GetHandler()->HandleRequest(request_.body(), response);
  }
  return response;
}

}  // namespace taraxa::net
