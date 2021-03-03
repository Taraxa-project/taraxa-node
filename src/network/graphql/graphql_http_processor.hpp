#pragma once

#include "network/http_processor.hpp"

// TODO: forward declare these classses
#include "chain/final_chain.hpp"
#include "dag/dag.hpp"
#include "mutation.hpp"
#include "query.hpp"
#include "subscription.hpp"

namespace taraxa::net {

class GraphQlHttpProcessor : public HttpProcessor {
 public:
  GraphQlHttpProcessor(const std::shared_ptr<FinalChain>& final_chain, const std::shared_ptr<DagManager>& dag_mgr);
  Response process(const Request& request) override;

 private:
  Response createErrResponse(std::string&& = "") override;
  Response createErrResponse(graphql::response::Value&& error_value);

 private:
  std::shared_ptr<graphql::taraxa::Query> query_;
  std::shared_ptr<graphql::taraxa::Mutation> mutation_;
  std::shared_ptr<graphql::taraxa::Subscription> subscription_;
  graphql::taraxa::Operations operations_;
};

}  // namespace taraxa::net
