#include "mutation.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include "account.hpp"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"
#include "libdevcore/CommonJS.h"
#include "transaction.hpp"

using namespace std::literals;

namespace graphql::taraxa {

Mutation::Mutation(std::shared_ptr<dev::rpc::Eth::NodeAPI> node_api) : node_api_(node_api) {}

service::FieldResult<response::Value> Mutation::applySendRawTransaction(service::FieldParams&&,
                                                                        response::Value&& transaction) const {
  return response::Value(
      dev::toJS(node_api_->importTransaction(jsToBytes(transaction.get<std::string>(), dev::OnFailed::Throw))));
}

service::FieldResult<response::Value> Mutation::applyTestMutation(service::FieldParams&& params,
                                                                  response::Value&& dataArg) const {
  std::cout << "applyTestMutation invoked" << std::endl;
  return response::Value(122);
}

service::FieldResult<response::Value> Mutation::applyTestMutation2(service::FieldParams&& params) const {
  std::cout << "applyTestMutation2 invoked" << std::endl;
  return response::Value(999);
}

}  // namespace graphql::taraxa
