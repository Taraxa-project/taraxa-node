#include "graphql/ws_server.hpp"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <libdevcore/CommonJS.h>

#include "common/jsoncpp.hpp"
#include "common/util.hpp"
#include "config/config.hpp"
#include "graphql/mutation.hpp"
#include "graphql/query.hpp"
#include "graphql/subscription.hpp"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"

namespace taraxa::net {

std::string GraphQlWsSession::processRequest(const std::string_view& /*request*/) {
  //  static auto q = std::make_shared<graphql::taraxa::Query>(ws_server_.lock()->getFinalChain(), nullptr, 0);
  //  static auto mutation = std::make_shared<graphql::taraxa::Mutation>();
  //  static auto subscription = std::make_shared<graphql::taraxa::Subscription>();
  //  static auto _service = std::make_shared<graphql::taraxa::Operations>(q, mutation, subscription);
  //
  //  using namespace graphql;
  //
  //  // graphql::peg::ast query;
  //  auto query = peg::parseString(request);
  //  response::Value variables(response::Type::Map);
  //  auto state = std::make_shared<graphql::service::RequestState>();
  //
  //  response::Value result;
  //
  //  if (request == "{triggerSubscription}") {
  //    std::cout << "graphql subscribe trigger start" << std::endl;
  //    // triggerTestSubscribtion(1234);
  //
  //    _service->deliver(std::launch::async, {"testSubscription"}, subscription);
  //
  //    std::cout << "graphql subscribe trigger end" << std::endl;
  //  } else if (request[0] == 's') {
  //    std::cout << "graphql subscribe start" << std::endl;
  //    service::SubscriptionParams subsParams =
  //        service::SubscriptionParams{nullptr, std::move(query), "", std::move(std::move(variables))};
  //
  //    service::SubscriptionKey key =
  //        _service->subscribe(std::move(subsParams), [](std::future<response::Value> payload) noexcept -> void {
  //          std::cout << "triggerTestSubscribtion: subscription callback body. payload: "
  //                    << response::toJSON(payload.get()) << std::endl;
  //          //         std::unique_lock<std::mutex> lock(spQueue->mutex);
  //          //
  //          //         if (!spQueue->registered)
  //          //         {
  //          //           return;
  //          //         }
  //          //
  //          //         spQueue->payloads.push(std::move(payload));
  //          //
  //          //         lock.unlock();
  //          //         spQueue->condition.notify_one();
  //        });
  //    result = response::Value(static_cast<int>(key));
  //    std::cout << "graphql subscribe end" << std::endl;
  //  } else {
  //    query = peg::parseString(request);
  //    result = _service->resolve(std::launch::async, state, query, "", std::move(variables)).get();
  //  }
  //
  //  return response::toJSON(std::move(result));

  return graphql::response::toJSON(graphql::response::Value("unimplemented"));
}

void GraphQlWsSession::triggerTestSubscribtion(unsigned int number) {
  Json::Value res, params;
  res["graphql"] = "2.0";
  res["method"] = "graphql_subscription";
  params["result"] = Json::Value(number);
  params["subscription"] = dev::toJS(0);
  res["params"] = params;
  auto response = util::to_string(res);
  ws_.text(ws_.got_text());
  do_write(std::move(response));
}

std::shared_ptr<WsSession> GraphQlWsServer::createSession(tcp::socket&& socket) {
  return std::make_shared<GraphQlWsSession>(std::move(socket), shared_from_this());
}

}  // namespace taraxa::net
