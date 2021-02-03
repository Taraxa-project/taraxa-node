#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql::taraxa {

class Block : public object::Block {
 public:
  explicit Block(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                 std::shared_ptr<dev::eth::BlockHeader> block_header_);

  virtual service::FieldResult<response::Value> getNumber(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getHash(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Block>> getParent(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getNonce(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getTransactionsRoot(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<response::IntType>> getTransactionCount(
      service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getStateRoot(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getReceiptsRoot(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Account>> getMiner(
      service::FieldParams&& params, std::optional<response::Value>&& blockArg) const override;
  virtual service::FieldResult<response::Value> getExtraData(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getGasLimit(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getGasUsed(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getTimestamp(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getLogsBloom(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getMixHash(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getDifficulty(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getTotalDifficulty(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<object::Transaction>>>> getTransactions(
      service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Transaction>> getTransactionAt(
      service::FieldParams&& params, response::IntType&& indexArg) const override;
  virtual service::FieldResult<std::shared_ptr<object::Account>> getAccount(
      service::FieldParams&& params, response::Value&& addressArg) const override;

 private:
  std::shared_ptr<dev::eth::BlockHeader> block_header_;
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
};

}  // namespace graphql::taraxa
