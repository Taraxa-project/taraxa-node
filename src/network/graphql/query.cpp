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
#include "common/types.hpp"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "graphqlservice/introspection/Introspection.h"
#include "libdevcore/CommonJS.h"
#include "log.hpp"
#include "transaction.hpp"
#include "types/current_state.hpp"
#include "types/dag_block.hpp"

using namespace std::literals;

namespace graphql::taraxa {

Query::Query(const std::shared_ptr<::taraxa::final_chain::FinalChain>& final_chain,
             const std::shared_ptr<::taraxa::DagManager>& dag_manager,
             const std::shared_ptr<::taraxa::DagBlockManager>& dag_block_manager,
             const std::shared_ptr<::taraxa::PbftManager>& pbft_manager,
             const std::shared_ptr<::taraxa::TransactionManager>& transaction_manager, uint64_t chain_id)
    : final_chain_(final_chain),
      dag_manager_(dag_manager),
      dag_block_manager_(dag_block_manager),
      pbft_manager_(pbft_manager),
      transaction_manager_(transaction_manager),
      chain_id_(chain_id) {}

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

service::FieldResult<std::shared_ptr<object::DagBlock>> Query::getDagBlock(
    service::FieldParams&& params, std::optional<response::Value>&& hashArg) const {
  std::shared_ptr<::taraxa::DagBlock> taraxa_dag_block = nullptr;

  if (hashArg) {
    taraxa_dag_block = dag_block_manager_->getDagBlock(::taraxa::blk_hash_t(hashArg->get<response::StringType>()));
  }
  // TODO: might be deleted - no need to return latest block in case no hash specified... for now it serves testing
  // purposes
  else {
    auto dag_blocks = dag_block_manager_->getDagBlocksAtLevel(dag_manager_->getMaxLevel());

    if (dag_blocks.size() > 0) {
      taraxa_dag_block = dag_blocks.front();
    }
  }

  return taraxa_dag_block
             ? std::make_shared<DagBlock>(taraxa_dag_block, final_chain_, pbft_manager_, transaction_manager_)
             : nullptr;
}

service::FieldResult<std::vector<std::shared_ptr<object::DagBlock>>> Query::getDagBlocks(
    service::FieldParams&& params, std::optional<response::Value>&& dagLevelArg,
    std::optional<response::IntType>&& countArg, std::optional<response::BooleanType>&& reverseArg) const {
  std::vector<std::shared_ptr<object::DagBlock>> dag_blocks_result;
  ::taraxa::level_t act_dag_level = dag_manager_->getMaxLevel();

  if (dagLevelArg) {
    act_dag_level = dagLevelArg->get<response::IntType>();
    if (act_dag_level < 0 || act_dag_level > dag_manager_->getMaxLevel()) {
      return dag_blocks_result;
    }
  }

  auto addDagBlocks = [&final_chain = final_chain_, &pbft_manager = pbft_manager_,
                       &transaction_manager = transaction_manager_](const auto& taraxa_dag_blocks,
                                                                    auto& result_dag_blocks) -> size_t {
    for (const auto& dag_block : taraxa_dag_blocks) {
      result_dag_blocks.push_back(
          std::make_shared<DagBlock>(dag_block, final_chain, pbft_manager, transaction_manager));
    }

    return taraxa_dag_blocks.size();
  };

  auto act_count = addDagBlocks(dag_block_manager_->getDagBlocksAtLevel(act_dag_level), dag_blocks_result);

  if (!countArg) {
    return dag_blocks_result;
  }

  auto count = std::min(countArg.value(), static_cast<int>(Query::MAX_PAGINATION_LIMIT));
  bool reverse_flag = reverseArg ? reverseArg.value() : false;

  while (act_count < count && act_dag_level <= dag_manager_->getMaxLevel()) {
    if (!reverse_flag) {
      act_dag_level++;
    } else if (act_dag_level > 0) {
      act_dag_level--;
    } else {
      return dag_blocks_result;
    }

    act_count += addDagBlocks(dag_block_manager_->getDagBlocksAtLevel(act_dag_level), dag_blocks_result);
  }

  return dag_blocks_result;
}

service::FieldResult<std::shared_ptr<object::CurrentState>> Query::getNodeState(service::FieldParams&& params) const {
  return std::make_shared<CurrentState>(final_chain_, dag_manager_);
}

}  // namespace graphql::taraxa