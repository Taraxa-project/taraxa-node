// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Block.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include "Account.h"
#include "Log.h"
#include "Transaction.h"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql {
namespace taraxa {

Block::Block(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
             std::shared_ptr<dev::eth::BlockHeader> block_header)
    : final_chain_(final_chain), block_header_(block_header) {}

service::FieldResult<response::Value> Block::getNumber(service::FieldParams&&) const {
  return response::Value(dev::toJS(block_header_->number()));
}

service::FieldResult<response::Value> Block::getHash(service::FieldParams&& params) const {
  return response::Value(block_header_->hash().toString());
}

service::FieldResult<std::shared_ptr<object::Block>> Block::getParent(service::FieldParams&&) const {
  return std::make_shared<Block>(final_chain_, std::make_shared<dev::eth::BlockHeader>(
                                                   final_chain_->blockHeader(dev::h256(block_header_->parentHash()))));
}

service::FieldResult<response::Value> Block::getNonce(service::FieldParams&&) const {
  return response::Value(block_header_->nonce().toString());
}

service::FieldResult<response::Value> Block::getTransactionsRoot(service::FieldParams&&) const {
  return response::Value(block_header_->transactionsRoot().toString());
}

service::FieldResult<std::optional<response::IntType>> Block::getTransactionCount(service::FieldParams&&) const {
  return std::optional<response::IntType>((response::IntType)final_chain_->transactionCount(block_header_->hash()));
}

service::FieldResult<response::Value> Block::getStateRoot(service::FieldParams&&) const {
  return response::Value(block_header_->stateRoot().toString());
}

service::FieldResult<response::Value> Block::getReceiptsRoot(service::FieldParams&&) const {
  return response::Value(block_header_->receiptsRoot().toString());
}

service::FieldResult<std::shared_ptr<object::Account>> Block::getMiner(service::FieldParams&&,
                                                                       std::optional<response::Value>&&) const {
  return std::make_shared<Account>(final_chain_, block_header_->author());
}

service::FieldResult<response::Value> Block::getExtraData(service::FieldParams&&) const {
  return response::Value(dev::toHex(block_header_->extraData()));
}

service::FieldResult<response::Value> Block::getGasLimit(service::FieldParams&&) const {
  return response::Value((response::IntType)block_header_->gasLimit());
}

service::FieldResult<response::Value> Block::getGasUsed(service::FieldParams&&) const {
  return response::Value((response::IntType)block_header_->gasUsed());
}

service::FieldResult<response::Value> Block::getTimestamp(service::FieldParams&&) const {
  return response::Value((response::IntType)block_header_->timestamp());
}

service::FieldResult<response::Value> Block::getLogsBloom(service::FieldParams&&) const {
  return response::Value(block_header_->logBloom().toString());
}

service::FieldResult<response::Value> Block::getMixHash(service::FieldParams&&) const {
  return response::Value(block_header_->mixHash().toString());
}

service::FieldResult<response::Value> Block::getDifficulty(service::FieldParams&&) const {
  return response::Value(block_header_->difficulty().str());
}

service::FieldResult<response::Value> Block::getTotalDifficulty(service::FieldParams&&) const {
  return response::Value(block_header_->difficulty().str());
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<object::Transaction>>>> Block::getTransactions(
    service::FieldParams&&) const {
  std::vector<std::shared_ptr<object::Transaction>> trxs;
  for (auto t : final_chain_->transactionHashes(block_header_->hash())) {
    trxs.push_back(std::make_shared<Transaction>(
        final_chain_, std::make_shared<dev::eth::Transaction>(final_chain_->transaction(t))));
  }
  return trxs;
}

service::FieldResult<std::shared_ptr<object::Transaction>> Block::getTransactionAt(service::FieldParams&&,
                                                                                   response::IntType&& index) const {
  return std::make_shared<Transaction>(
      final_chain_, std::make_shared<dev::eth::Transaction>(
                        final_chain_->transaction(final_chain_->transactionHashes(block_header_->hash())[index])));
}

service::FieldResult<std::shared_ptr<object::Account>> Block::getAccount(service::FieldParams&&,
                                                                         response::Value&&) const {
  return std::make_shared<Account>(final_chain_, block_header_->author());
}

} /* namespace taraxa */
} /* namespace graphql */
