#pragma once

#include <memory>
#include <string>

#include "LogObject.h"
#include "final_chain/final_chain.hpp"
#include "transaction/transaction_manager.hpp"

namespace graphql::taraxa {

class Log {
 public:
  explicit Log(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
               std::shared_ptr<::taraxa::TransactionManager> trx_manager, ::taraxa::final_chain::LogEntry log) noexcept;

  int getIndex() const noexcept;
  std::shared_ptr<object::Account> getAccount(std::optional<response::Value>&& blockArg) const noexcept;
  std::vector<response::Value> getTopics() const noexcept;
  response::Value getData() const noexcept;
  std::shared_ptr<object::Transaction> getTransaction() const noexcept;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::TransactionManager> trx_manager_;
  const ::taraxa::final_chain::LogEntry kLog;
};

}  // namespace graphql::taraxa