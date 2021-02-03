#include "Account.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql {
namespace taraxa {

Account::Account(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::Address address)
    : address_(address), final_chain_(final_chain) {
  account_ = final_chain->get_account(address_, final_chain->last_block_number());
}

service::FieldResult<response::Value> Account::getAddress(service::FieldParams&&) const {
  return response::Value(address_.toString());
}

service::FieldResult<response::Value> Account::getBalance(service::FieldParams&&) const {
  if (account_) {
    return response::Value(dev::toJS(account_->Balance));
  }
  return response::Value(0);
}

service::FieldResult<response::Value> Account::getTransactionCount(service::FieldParams&&) const {
  if (account_) {
    return response::Value(dev::toJS(account_->Nonce));
  }
  return response::Value(0);
}

service::FieldResult<response::Value> Account::getCode(service::FieldParams&&) const {
  return response::Value(dev::toJS(final_chain_->get_code(address_, final_chain_->last_block_number())));
}

service::FieldResult<response::Value> Account::getStorage(service::FieldParams&&, response::Value&& slotArg) const {
  return response::Value(dev::toJS(final_chain_->get_account_storage(address_, dev::u256(slotArg.get<std::string>()))));
}

} /* namespace taraxa */
} /* namespace graphql */
