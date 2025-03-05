#pragma once

#include <memory>
#include <string>
#include <vector>

#include "TransactionObject.h"
#include "final_chain/final_chain.hpp"
#include "transaction/transaction_manager.hpp"

namespace graphql::taraxa {

class Transaction final : public std::enable_shared_from_this<Transaction> {
 public:
  explicit Transaction(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                       std::shared_ptr<::taraxa::TransactionManager> trx_manager,
                       std::function<std::shared_ptr<object::Block>(::taraxa::EthBlockNumber)>,
                       std::shared_ptr<::taraxa::Transaction> transaction) noexcept;

  response::Value getHash() const noexcept;
  response::Value getNonce() const noexcept;
  std::optional<int> getIndex() const noexcept;
  std::shared_ptr<object::Account> getFrom(std::optional<response::Value>&& blockArg) const;
  std::shared_ptr<object::Account> getTo(std::optional<response::Value>&& blockArg) const;
  response::Value getValue() const noexcept;
  response::Value getGasPrice() const noexcept;
  response::Value getGas() const noexcept;
  response::Value getInputData() const noexcept;
  std::shared_ptr<object::Block> getBlock() const;
  std::optional<response::Value> getStatus() const noexcept;
  std::optional<response::Value> getGasUsed() const noexcept;
  std::optional<response::Value> getCumulativeGasUsed() const noexcept;
  std::shared_ptr<object::Account> getCreatedContract(std::optional<response::Value>&& blockArg) const noexcept;
  std::optional<std::vector<std::shared_ptr<object::Log>>> getLogs() const noexcept;
  response::Value getR() const noexcept;
  response::Value getS() const noexcept;
  response::Value getV() const noexcept;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::TransactionManager> trx_manager_;
  std::function<std::shared_ptr<object::Block>(::taraxa::EthBlockNumber)> get_block_by_num_;
  std::shared_ptr<::taraxa::Transaction> transaction_;
  // Caching for performance
  mutable std::optional<::taraxa::TransactionReceipt> receipt_;
  mutable std::optional<::taraxa::TransactionLocation> location_;
};

}  // namespace graphql::taraxa