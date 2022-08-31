#pragma once

#include <memory>
#include <string>

#include "BlockObject.h"
#include "final_chain/final_chain.hpp"
#include "transaction/transaction_manager.hpp"

namespace graphql::taraxa {

class Block {
 public:
  explicit Block(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                 std::shared_ptr<::taraxa::TransactionManager> trx_manager,
                 std::shared_ptr<const ::taraxa::final_chain::BlockHeader> block_header) noexcept;

  response::Value getNumber() const noexcept;
  response::Value getHash() const noexcept;
  std::shared_ptr<object::Block> getParent() const noexcept;
  response::Value getNonce() const noexcept;
  response::Value getTransactionsRoot() const noexcept;
  std::optional<int> getTransactionCount() const noexcept;
  response::Value getStateRoot() const noexcept;
  response::Value getReceiptsRoot() const noexcept;
  std::shared_ptr<object::Account> getMiner(std::optional<response::Value>&& blockArg) const noexcept;
  response::Value getExtraData() const noexcept;
  response::Value getGasLimit() const noexcept;
  response::Value getGasUsed() const noexcept;
  response::Value getTimestamp() const noexcept;
  response::Value getLogsBloom() const noexcept;
  response::Value getMixHash() const noexcept;
  response::Value getDifficulty() const noexcept;
  response::Value getTotalDifficulty() const noexcept;
  std::optional<int> getOmmerCount() const noexcept;
  std::optional<std::vector<std::shared_ptr<object::Block>>> getOmmers() const noexcept;
  std::shared_ptr<object::Block> getOmmerAt(int&& indexArg) const noexcept;
  response::Value getOmmerHash() const noexcept;
  std::optional<std::vector<std::shared_ptr<object::Transaction>>> getTransactions() const noexcept;
  std::shared_ptr<object::Transaction> getTransactionAt(response::IntType&& indexArg) const noexcept;
  std::vector<std::shared_ptr<object::Log>> getLogs(BlockFilterCriteria&& filterArg) const noexcept;
  std::shared_ptr<object::Account> getAccount(response::Value&& addressArg) const noexcept;
  std::shared_ptr<object::CallResult> getCall(CallData&& dataArg) const noexcept;
  response::Value getEstimateGas(CallData&& dataArg) const noexcept;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::TransactionManager> trx_manager_;
  std::shared_ptr<const ::taraxa::final_chain::BlockHeader> block_header_;
};

}  // namespace graphql::taraxa