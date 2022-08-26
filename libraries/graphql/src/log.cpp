#include "graphql/log.hpp"

#include <optional>

#include "graphql/account.hpp"
#include "graphql/transaction.hpp"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql::taraxa {

Log::Log(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
         std::shared_ptr<::taraxa::TransactionManager> trx_manager, ::taraxa::final_chain::LogEntry log) noexcept
    : final_chain_(std::move(final_chain)), trx_manager_(std::move(trx_manager)), kLog(std::move(log)) {}

int Log::getIndex() const noexcept { return 0; }

std::shared_ptr<object::Account> Log::getAccount(std::optional<response::Value>&&) const noexcept {
  return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, kLog.address));
}

std::vector<response::Value> Log::getTopics() const noexcept {
  std::vector<response::Value> ret;
  ret.reserve(kLog.topics.size());
  for (auto t : kLog.topics) ret.push_back(response::Value(dev::toJS(t)));
  return ret;
}

response::Value Log::getData() const noexcept { return response::Value(dev::toJS(kLog.data)); }

std::shared_ptr<object::Transaction> Log::getTransaction() const noexcept { return nullptr; }

}  // namespace graphql::taraxa