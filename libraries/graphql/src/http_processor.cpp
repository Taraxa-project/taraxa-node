#include "http_processor.hpp"

#include <jsoncpp/json/reader.h>

#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"

namespace taraxa::net {

using namespace graphql;

GraphQlHttpProcessor::GraphQlHttpProcessor(const std::shared_ptr<::taraxa::final_chain::FinalChain>& final_chain,
                                           const std::shared_ptr<::taraxa::DagManager>& dag_manager,
                                           const std::shared_ptr<::taraxa::DagBlockManager>& dag_block_manager,
                                           const std::shared_ptr<::taraxa::PbftManager>& pbft_manager,
                                           const std::shared_ptr<::taraxa::TransactionManager>& transaction_manager)
    : HttpProcessor(),
      query_(std::make_shared<graphql::taraxa::Query>(final_chain, dag_manager, dag_block_manager, pbft_manager,
                                                      transaction_manager, 0)),
      mutation_(std::make_shared<graphql::taraxa::Mutation>()),
      subscription_(std::make_shared<graphql::taraxa::Subscription>()),
      operations_(query_, mutation_, subscription_) {}

HttpProcessor::Response GraphQlHttpProcessor::process(const Request& request) {
  try {
    const std::string& request_str = request.body();

    peg::ast query_ast;
    response::Value variables{response::Type::Map};
    std::string operation_name{""};

    // Differenciate content type: 'application/json' vs 'application/graphql'
    // according to https://graphql.org/learn/serving-over-http/
    if (request.count("Content-Type") && request["Content-Type"] == "application/graphql") {
      query_ast = peg::parseString(request_str);
    } else {  // default is "application/json"
      Json::Reader reader;
      Json::Value json_request;

      if (!reader.parse(request_str, json_request, false)) {
        return createErrResponse("Unable to parse request json data");
      }

      const std::string query_key{service::strQuery};
      if (!json_request.isMember(query_key)) {
        return createErrResponse("Invalid request json data: Missing \'query\' in graphql request");
      }
      query_ast = peg::parseString(json_request[query_key].asString());

      // operationName field is optional
      if (json_request.isMember("operationName")) {
        operation_name = json_request["operationName"].asString();
      }

      // variables field is optional
      if (json_request.isMember("variables")) {
        variables = response::parseJSON(json_request["variables"].toStyledString());
        if (variables.type() != response::Type::Map) {
          return createErrResponse("Invalid request  json data: Variables object must be of map type");
        }
      }
    }

    auto result =
        operations_.resolve(std::launch::async, nullptr, query_ast, operation_name, std::move(variables)).get();
    return createOkResponse(response::toJSON(std::move(result)));

  } catch (const Json::Exception& e) {
    return createErrResponse("Unable to process request json data: " + std::string(e.what()));
  } catch (service::schema_exception& e) {
    return createErrResponse(e.getErrors());
  } catch (const std::exception& e) {
    return createErrResponse("Unable to process request: " + std::string(e.what()));
  }
}

HttpProcessor::Response GraphQlHttpProcessor::createErrResponse(std::string&& response_body) {
  // std::string graphql_er_json = "{\"data\":null,\"errors\":[{\"message\":\"" + response_body + "\"}]}";
  response::Value error(response::Type::Map);
  error.emplace_back(std::string{service::strMessage}, response::Value(std::move(response_body)));

  response::Value errors(response::Type::List);
  errors.emplace_back(std::move(error));

  response::Value response(response::Type::Map);
  response.emplace_back(std::string{service::strData}, response::Value());
  response.emplace_back(std::string{service::strErrors}, std::move(errors));

  return HttpProcessor::createOkResponse(response::toJSON(std::move(response)));
}

HttpProcessor::Response GraphQlHttpProcessor::createErrResponse(response::Value&& error_value) {
  response::Value response(response::Type::Map);
  response.emplace_back(std::string{service::strData}, response::Value());
  response.emplace_back(std::string{service::strErrors}, std::move(error_value));

  return HttpProcessor::createOkResponse(response::toJSON(std::move(response)));
}

}  // namespace taraxa::net