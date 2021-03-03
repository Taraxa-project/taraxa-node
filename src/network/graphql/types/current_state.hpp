#pragma once

#include <memory>

//#include "chain/final_chain.hpp"
//#include "chain/state_api.hpp"
#include "chain/final_chain.hpp"
#include "dag/dag.hpp"
#include "network/graphql/gen/TaraxaSchema.h"

namespace graphql::taraxa {

class CurrentState : public object::CurrentState {
 public:
  // TODO: refactor/optimize(const ref, ...) constructor parameters in all graphql types
  explicit CurrentState(const std::shared_ptr<::taraxa::final_chain::FinalChain>& final_chain,
                        const std::shared_ptr<::taraxa::DagManager>& dag_mgr);

  virtual service::FieldResult<response::Value> getFinalBlock(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getDagBlockLevel(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getDagBlockPeriod(service::FieldParams&& params) const override;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::DagManager> dag_mgr_;
};

}  // namespace graphql::taraxa
