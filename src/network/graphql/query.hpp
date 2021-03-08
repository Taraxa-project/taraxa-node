#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
#include "gen/TaraxaSchema.h"
#include "transaction_manager/transaction_manager.hpp"

namespace graphql::taraxa {

class Query : public object::Query {
 public:
  explicit Query(const std::shared_ptr<::taraxa::final_chain::FinalChain>& final_chain,
                 const std::shared_ptr<::taraxa::DagManager>& dag_manager,
                 const std::shared_ptr<::taraxa::DagBlockManager>& dag_block_manager,
                 const std::shared_ptr<::taraxa::PbftManager>& pbft_manager,
                 const std::shared_ptr<::taraxa::TransactionManager>& transaction_manager, uint64_t chain_id);

  virtual service::FieldResult<std::shared_ptr<object::Block>> getBlock(
      service::FieldParams&& params, std::optional<response::Value>&& numberArg,
      std::optional<response::Value>&& hashArg) const override;
  virtual service::FieldResult<std::vector<std::shared_ptr<object::Block>>> getBlocks(
      service::FieldParams&& params, response::Value&& fromArg, std::optional<response::Value>&& toArg) const override;
  virtual service::FieldResult<std::shared_ptr<object::Transaction>> getTransaction(
      service::FieldParams&& params, response::Value&& hashArg) const override;
  virtual service::FieldResult<response::Value> getGasPrice(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getChainID(service::FieldParams&& params) const override;
  //  virtual service::FieldResult<std::shared_ptr<object::SyncState>> getSyncing(
  //      service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::DagBlock>> getDagBlock(
      service::FieldParams&& params, std::optional<response::Value>&& hashArg) const override;
  virtual service::FieldResult<std::vector<std::shared_ptr<object::DagBlock>>> getDagBlocks(
      service::FieldParams&& params, std::optional<response::Value>&& dagLevelArg,
      std::optional<response::IntType>&& countArg, std::optional<response::BooleanType>&& reverseArg) const override;
  virtual service::FieldResult<std::shared_ptr<object::CurrentState>> getNodeState(
      service::FieldParams&& params) const override;

 private:
  // TODO: use pagination limit for all "list" queries
  static constexpr size_t MAX_PAGINATION_LIMIT{100};

  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::DagManager> dag_manager_;
  std::shared_ptr<::taraxa::DagBlockManager> dag_block_manager_;
  std::shared_ptr<::taraxa::PbftManager> pbft_manager_;
  std::shared_ptr<::taraxa::TransactionManager> transaction_manager_;
  uint64_t chain_id_;
};

}  // namespace graphql::taraxa