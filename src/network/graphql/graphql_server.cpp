#include "graphql_server.hpp"

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

std::shared_ptr<HttpConnection> GraphQlServer::createConnection() {
  return std::make_shared<GraphQlConnection>(getShared());
}

std::string GraphQlConnection::processRequest(std::string request) {
  auto q = std::make_shared<graphql::taraxa::Query>(http_->getFinalChain(), 0);
  auto mutation = std::make_shared<graphql::taraxa::Mutation>();
  auto _service = std::make_shared<graphql::taraxa::Operations>(q, mutation);

  using namespace graphql;
  auto query = peg::parseString(request);
  response::Value variables(response::Type::Map);
  auto state = std::make_shared<graphql::service::RequestState>();

  auto result = _service->resolve(std::launch::async, state, query, "", std::move(variables)).get();

  return response::toJSON(response::Value(result));
}

}  // namespace taraxa::net
