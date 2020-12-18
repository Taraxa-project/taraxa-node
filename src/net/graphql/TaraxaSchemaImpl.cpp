// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "TaraxaSchemaImpl.h"

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

using namespace std::literals;

namespace graphql {
namespace taraxa {

Query::Query(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain) : final_chain_(final_chain) {}

service::FieldResult<std::shared_ptr<object::Block>> Query::getBlock(service::FieldParams&&,
                                                                     std::optional<response::Value>&& number,
                                                                     std::optional<response::Value>&& hash) const {
  if (number) {
    return std::make_shared<Block>(
        std::make_shared<dev::eth::BlockHeader>(final_chain_->blockHeader(number->get<int>())));
  }
  return std::make_shared<Block>(final_chain_->get_last_block());
}

Mutation::Mutation() {}

Block::Block(std::shared_ptr<dev::eth::BlockHeader> block_header) : block_header_(block_header) {}

service::FieldResult<response::Value> Block::getNumber(service::FieldParams&&) const {
  return response::Value((int)block_header_->number());
}

service::FieldResult<response::Value> Block::getHash(service::FieldParams&& params) const {
  return response::Value(block_header_->hash().toString());
}

} /* namespace taraxa */
} /* namespace graphql */
