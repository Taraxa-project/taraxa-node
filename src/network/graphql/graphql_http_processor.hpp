#pragma once

#include "network/http_processor.hpp"

// TODO: forward declare these classses
#include "chain/final_chain.hpp"
#include "mutation.hpp"
#include "query.hpp"

namespace taraxa::net {

class GraphQlHttpProcessor : public HttpProcessor {
 public:
  GraphQlHttpProcessor(const std::shared_ptr<FinalChain>& final_chain);
  Response process(const Request& request) override;

 private:
  Response createErrResponse(const std::string& response_body = "") override;

 private:
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<graphql::taraxa::Query> query_;
  std::shared_ptr<graphql::taraxa::Mutation> mutation_;
  graphql::taraxa::Operations operations_;
};

}  // namespace taraxa::net