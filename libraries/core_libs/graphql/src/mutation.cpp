#include "graphql/mutation.hpp"

#include "graphql/account.hpp"
#include "graphql/transaction.hpp"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql::taraxa {

Mutation::Mutation(std::shared_ptr<::taraxa::net::rpc::eth::Eth> node_api) noexcept : node_api_(std::move(node_api)) {}

response::Value Mutation::applySendRawTransaction(response::Value&&) const noexcept {
  // return response::Value(
  //     dev::toJS(node_api_->importTransaction(jsToBytes(transaction.get<std::string>(), dev::OnFailed::Throw))));
  return response::Value(666);
}

response::Value Mutation::applyTestMutation(response::Value&&) const noexcept {
  std::cout << "applyTestMutation invoked" << std::endl;
  return response::Value(122);
}

response::Value Mutation::applyTestMutation2() const noexcept {
  std::cout << "applyTestMutation2 invoked" << std::endl;
  return response::Value(999);
}

}  // namespace graphql::taraxa