#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql::taraxa {

class Log : public object::Log {
 public:
  explicit Log(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::eth::LocalisedLogEntry log);

  virtual service::FieldResult<response::IntType> getIndex(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Account>> getAccount(
      service::FieldParams&& params, std::optional<response::Value>&& blockArg) const override;
  virtual service::FieldResult<std::vector<response::Value>> getTopics(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getData(service::FieldParams&& params) const override;
  virtual service::FieldResult<std::shared_ptr<object::Transaction>> getTransaction(
      service::FieldParams&& params) const override;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  dev::eth::LocalisedLogEntry log_;
};

}  // namespace graphql::taraxa
