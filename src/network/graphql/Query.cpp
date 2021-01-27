// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Query.h"

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
#include "Transaction.h"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"

using namespace std::literals;

namespace graphql {
namespace taraxa {

Query::Query(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, uint64_t chain_id)
    : final_chain_(final_chain), chain_id_(chain_id) {}

service::FieldResult<std::shared_ptr<object::Block>> Query::getBlock(service::FieldParams&&,
                                                                     std::optional<response::Value>&& number,
                                                                     std::optional<response::Value>&& hash) const {
  if (number) {
    return std::make_shared<Block>(
        final_chain_, std::make_shared<dev::eth::BlockHeader>(final_chain_->blockHeader(number->get<int>())));
  }
  if (hash) {
    return std::make_shared<Block>(final_chain_, std::make_shared<dev::eth::BlockHeader>(
                                                     final_chain_->blockHeader(dev::h256(hash->get<std::string>()))));
  }
  return std::make_shared<Block>(final_chain_, final_chain_->get_last_block());
}

service::FieldResult<std::vector<std::shared_ptr<object::Block>>> Query::getBlocks(
    service::FieldParams&& params, response::Value&& fromArg, std::optional<response::Value>&& toArg) const {
  std::vector<std::shared_ptr<object::Block>> blocks;
  for (auto i = fromArg.get<int>(); i <= toArg->get<int>(); i++) {
    blocks.push_back(
        std::make_shared<Block>(final_chain_, std::make_shared<dev::eth::BlockHeader>(final_chain_->blockHeader(i))));
  }
  return blocks;
}

service::FieldResult<std::shared_ptr<object::Transaction>> Query::getTransaction(service::FieldParams&& params,
                                                                                 response::Value&& hashArg) const {
  return std::make_shared<Transaction>(final_chain_, std::make_shared<dev::eth::Transaction>(final_chain_->transaction(
                                                         ::taraxa::trx_hash_t(hashArg.get<std::string>()))));
}

service::FieldResult<response::Value> Query::getGasPrice(service::FieldParams&& params) const {
  return response::Value(0);
}

service::FieldResult<response::Value> Query::getChainID(service::FieldParams&& params) const {
  return response::Value(dev::toJS(chain_id_));
}

Mutation::Mutation() {}

} /* namespace taraxa */
} /* namespace graphql */
