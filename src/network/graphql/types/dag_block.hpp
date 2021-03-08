#pragma once

#include "chain/final_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "network/graphql/gen/TaraxaSchema.h"
#include "transaction_manager/transaction_manager.hpp"

namespace graphql::taraxa {

class DagBlock : public object::DagBlock {
 public:
  explicit DagBlock(const std::shared_ptr<::taraxa::DagBlock>& dag_block,
                    const std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                    const std::shared_ptr<::taraxa::PbftManager>& pbft_manager,
                    const std::shared_ptr<::taraxa::TransactionManager>& transaction_manager);

  virtual service::FieldResult<response::Value> getHash(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getPivot(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::vector<response::Value>> getTips(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getLevel(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<response::Value>> getPbftPeriod(
      service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Account>> getAuthor(
      service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getTimestamp(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<object::Transaction>>>> getTransactions(
      service::FieldParams&& params) const override;

 private:
  std::shared_ptr<::taraxa::DagBlock> dag_block_;
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::PbftManager> pbft_manager_;
  std::shared_ptr<::taraxa::TransactionManager> transaction_manager_;
};

}  // namespace graphql::taraxa
