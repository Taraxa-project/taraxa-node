// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Transaction.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include "Account.h"
#include "Block.h"
#include "Log.h"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql {
namespace taraxa {

Transaction::Transaction(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,

                         std::shared_ptr<dev::eth::Transaction> transaction)
    : transaction_(transaction), final_chain_(final_chain) {}

service::FieldResult<response::Value> Transaction::getHash(service::FieldParams&&) const {
  return response::Value(transaction_->sha3().toString());
}

service::FieldResult<response::Value> Transaction::getNonce(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->nonce()));
}

service::FieldResult<std::optional<response::IntType>> Transaction::getIndex(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getIndex is not implemented)ex");
}

service::FieldResult<std::shared_ptr<object::Account>> Transaction::getFrom(service::FieldParams&&,
                                                                            std::optional<response::Value>&&) const {
  return std::make_shared<Account>(final_chain_, transaction_->from());
}

service::FieldResult<std::shared_ptr<object::Account>> Transaction::getTo(service::FieldParams&&,
                                                                          std::optional<response::Value>&&) const {
  return std::make_shared<Account>(final_chain_, transaction_->to());
}

service::FieldResult<response::Value> Transaction::getValue(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->value()));
}

service::FieldResult<response::Value> Transaction::getGasPrice(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->gasPrice()));
}

service::FieldResult<response::Value> Transaction::getGas(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->gas()));
}

service::FieldResult<std::shared_ptr<object::Block>> Transaction::getBlock(service::FieldParams&&) const {
  if (!final_chain_->isKnownTransaction(transaction_->sha3())) return nullptr;
  return std::make_shared<Block>(final_chain_,
                                 std::make_shared<dev::eth::BlockHeader>(final_chain_->blockHeader(
                                     final_chain_->localisedTransactionReceipt(transaction_->sha3()).blockHash())));
}

service::FieldResult<std::optional<response::Value>> Transaction::getGasUsed(service::FieldParams&&) const {
  return response::Value(dev::toJS(final_chain_->localisedTransactionReceipt(transaction_->sha3()).gasUsed()));
}

service::FieldResult<std::optional<response::Value>> Transaction::getCumulativeGasUsed(service::FieldParams&&) const {
  return response::Value(
      dev::toJS(final_chain_->localisedTransactionReceipt(transaction_->sha3()).cumulativeGasUsed()));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<object::Log>>>> Transaction::getLogs(
    service::FieldParams&&) const {
  std::vector<std::shared_ptr<object::Log>> logs;
  for (auto log : final_chain_->localisedTransactionReceipt(transaction_->sha3()).localisedLogs())
    logs.push_back(std::make_shared<Log>(final_chain_, log));
  return logs;
}

service::FieldResult<response::Value> Transaction::getR(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->signature().r));
}

service::FieldResult<response::Value> Transaction::getS(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->signature().s));
}

service::FieldResult<response::Value> Transaction::getV(service::FieldParams&&) const {
  return response::Value(dev::toJS(transaction_->signature().v));
}

} /* namespace taraxa */
} /* namespace graphql */
