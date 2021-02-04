#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql::taraxa {

class Account : public object::Account {
 public:
  explicit Account(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::Address address);

  virtual service::FieldResult<response::Value> getAddress(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getBalance(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getTransactionCount(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getCode(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getStorage(service::FieldParams&& params,
                                                           response::Value&& slotArg) const override;

 private:
  dev::Address address_;
  std::optional<::taraxa::state_api::Account> account_;
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
};
}  // namespace graphql::taraxa