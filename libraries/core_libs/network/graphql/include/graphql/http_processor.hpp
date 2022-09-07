#pragma once

#include "dag/dag.hpp"
#include "final_chain/final_chain.hpp"
#include "mutation.hpp"
#include "network/http_server.hpp"
#include "query.hpp"
#include "subscription.hpp"

namespace taraxa::net {

class GraphQlHttpProcessor final : public HttpProcessor {
 public:
  GraphQlHttpProcessor(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                       std::shared_ptr<::taraxa::DagManager> dag_manager,
                       std::shared_ptr<::taraxa::PbftManager> pbft_manager,
                       std::shared_ptr<::taraxa::TransactionManager> transaction_manager,
                       std::shared_ptr<::taraxa::DbStorage> db, uint64_t chain_id);
  Response process(const Request& request) override;

 private:
  Response createErrResponse(std::string&& = "");
  Response createErrResponse(graphql::response::Value&& error_value);
  Response createOkResponse(std::string&& response_body);

 private:
  std::shared_ptr<graphql::taraxa::Query> query_;
  std::shared_ptr<graphql::taraxa::Mutation> mutation_;
  std::shared_ptr<graphql::taraxa::Subscription> subscription_;
  graphql::taraxa::Operations operations_;
};

}  // namespace taraxa::net