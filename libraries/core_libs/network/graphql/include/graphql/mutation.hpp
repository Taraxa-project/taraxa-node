#pragma once

#include <memory>

#include "MutationObject.h"
#include "transaction/transaction_manager.hpp"

namespace graphql::taraxa {

class Mutation {
 public:
  Mutation() = default;
  explicit Mutation(std::shared_ptr<::taraxa::TransactionManager> trx_manager) noexcept;

  response::Value applySendRawTransaction(response::Value&& dataArg) const;

 private:
  std::shared_ptr<::taraxa::TransactionManager> trx_manager_;
};

}  // namespace graphql::taraxa