#include "graphql_ws_server.hpp"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <libdevcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include "config/config.hpp"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "network/graphql/mutation.hpp"
#include "network/graphql/query.hpp"
#include "util/util.hpp"

namespace taraxa::net {

std::string GraphQlWSSession::processRequest(std::string request) {
  auto q = std::make_shared<graphql::taraxa::Query>(ws_server_.lock()->getFinalChain(), 0);
  auto mutation = std::make_shared<graphql::taraxa::Mutation>();
  auto _service = std::make_shared<graphql::taraxa::Operations>(q, mutation);

  using namespace graphql;
  auto query = peg::parseString(request);
  response::Value variables(response::Type::Map);
  auto state = std::make_shared<graphql::service::RequestState>();

  auto result = _service->resolve(std::launch::async, state, query, "", std::move(variables)).get();

  return response::toJSON(response::Value(result));
}

std::shared_ptr<WSSession> GraphQlWsServer::createSession(tcp::socket &socket) {
  return std::make_shared<GraphQlWSSession>(std::move(socket), node_addr_, shared_from_this());
}

}  // namespace taraxa::net