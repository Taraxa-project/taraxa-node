#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql::taraxa {

class Transaction : public object::Transaction {
 public:
  explicit Transaction(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                       std::shared_ptr<dev::eth::Transaction> transaction);

  virtual service::FieldResult<response::Value> getHash(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getNonce(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<response::IntType>> getIndex(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Account>> getFrom(
      service::FieldParams&& params, std::optional<response::Value>&& blockArg) const override;
  virtual service::FieldResult<std::shared_ptr<object::Account>> getTo(
      service::FieldParams&& params, std::optional<response::Value>&& blockArg) const override;
  virtual service::FieldResult<response::Value> getValue(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getGasPrice(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getGas(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Block>> getBlock(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<response::Value>> getGasUsed(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<response::Value>> getCumulativeGasUsed(
      service::FieldParams&& params) const override;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<object::Log>>>> getLogs(
      service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getR(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getS(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getV(service::FieldParams&& params) const override;

 private:
  std::shared_ptr<dev::eth::Transaction> transaction_;
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
};

}  // namespace graphql::taraxa
