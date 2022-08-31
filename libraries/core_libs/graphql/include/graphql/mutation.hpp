#pragma once

#include <memory>

#include "MutationObject.h"
#include "network/rpc/eth/Eth.h"

namespace graphql::taraxa {

class Mutation {
 public:
  Mutation() = default;
  explicit Mutation(std::shared_ptr<::taraxa::net::rpc::eth::Eth> node_api) noexcept;

  response::Value applySendRawTransaction(response::Value&& dataArg) const noexcept;

  response::Value applyTestMutation(response::Value&& dataArg) const noexcept;

  response::Value applyTestMutation2() const noexcept;

 private:
  std::shared_ptr<::taraxa::net::rpc::eth::Eth> node_api_;
};

}  // namespace graphql::taraxa