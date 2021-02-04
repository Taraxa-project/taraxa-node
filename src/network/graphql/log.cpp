#include "log.hpp"

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
#include "log.hpp"
#include "transaction.hpp"

using namespace std::literals;

namespace graphql::taraxa {

Log::Log(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::eth::LocalisedLogEntry log)
    : final_chain_(final_chain), log_(log) {}

service::FieldResult<response::IntType> Log::getIndex(service::FieldParams&&) const {
  return response::IntType(log_.logIndex);
}

service::FieldResult<std::shared_ptr<object::Account>> Log::getAccount(service::FieldParams&&,
                                                                       std::optional<response::Value>&&) const {
  return std::make_shared<Account>(final_chain_, log_.address);
}

service::FieldResult<std::vector<response::Value>> Log::getTopics(service::FieldParams&&) const {
  std::vector<response::Value> ret;
  for (auto t : log_.topics) ret.push_back(response::Value(dev::toJS(t)));
  return ret;
}

service::FieldResult<response::Value> Log::getData(service::FieldParams&&) const {
  return response::Value(dev::toJS(log_.data));
}

service::FieldResult<std::shared_ptr<object::Transaction>> Log::getTransaction(service::FieldParams&&) const {
  return std::make_shared<Transaction>(
      final_chain_, std::make_shared<dev::eth::Transaction>(final_chain_->transaction(log_.transactionHash)));
}

}  // namespace graphql::taraxa
