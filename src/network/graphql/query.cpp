#include "query.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include "account.hpp"
#include "block.hpp"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"
#include "libdevcore/CommonJS.h"
#include "log.hpp"
#include "transaction.hpp"
#include "types/current_state.hpp"

using namespace std::literals;

namespace graphql::taraxa {

Query::Query(const std::shared_ptr<::taraxa::final_chain::FinalChain>& final_chain,
             const std::shared_ptr<::taraxa::DagManager>& dag_mgr, uint64_t chain_id)
    : final_chain_(final_chain), dag_mgr_(dag_mgr), chain_id_(chain_id) {}

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

  // TODO: use pagination limit for all "list" queries
  uint64_t start_block_num = fromArg.get<int>();
  uint64_t end_block_num = toArg ? toArg->get<int>() : Query::MAX_PAGINATION_LIMIT;

  if (end_block_num - start_block_num > Query::MAX_PAGINATION_LIMIT) {
    end_block_num = start_block_num + Query::MAX_PAGINATION_LIMIT;
  }

  for (uint64_t block_num = start_block_num; block_num <= end_block_num; block_num++) {
    if (!final_chain_->isKnown(block_num)) {
      break;
    }

    blocks.push_back(std::make_shared<Block>(
        final_chain_, std::make_shared<dev::eth::BlockHeader>(final_chain_->blockHeader(block_num))));
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

service::FieldResult<std::shared_ptr<object::CurrentState>> Query::getNodeState(service::FieldParams&& params) const {
  return std::make_shared<CurrentState>(final_chain_, dag_mgr_);
}

}  // namespace graphql::taraxa