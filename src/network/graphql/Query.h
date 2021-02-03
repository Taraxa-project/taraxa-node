#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql::taraxa {

class Query : public object::Query {
 public:
  explicit Query(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, uint64_t chain_id);

  virtual service::FieldResult<std::shared_ptr<object::Block>> getBlock(
      service::FieldParams&& params, std::optional<response::Value>&& numberArg,
      std::optional<response::Value>&& hashArg) const override;
  virtual service::FieldResult<std::vector<std::shared_ptr<object::Block>>> getBlocks(
      service::FieldParams&& params, response::Value&& fromArg, std::optional<response::Value>&& toArg) const override;
  virtual service::FieldResult<std::shared_ptr<object::Transaction>> getTransaction(
      service::FieldParams&& params, response::Value&& hashArg) const override;
  virtual service::FieldResult<response::Value> getGasPrice(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getChainID(service::FieldParams&& params) const override;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  uint64_t chain_id_;
};

class Mutation : public object::Mutation {
 public:
  explicit Mutation();

 private:
};
}  // namespace graphql::taraxa