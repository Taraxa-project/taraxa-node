#pragma once

#include "DagBlockObject.h"
#include "final_chain/final_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"

namespace graphql::taraxa {

class DagBlock {
 public:
  explicit DagBlock(std::shared_ptr<::taraxa::DagBlock> dag_block,
                    std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                    std::shared_ptr<::taraxa::PbftManager> pbft_manager,
                    std::shared_ptr<::taraxa::TransactionManager> transaction_manager,
                    std::function<std::shared_ptr<object::Block>(::taraxa::EthBlockNumber)> get_block_by_num) noexcept;

  response::Value getHash() const noexcept;
  response::Value getPivot() const noexcept;
  std::vector<response::Value> getTips() const noexcept;
  response::Value getLevel() const noexcept;
  std::optional<response::Value> getPbftPeriod() const noexcept;
  std::shared_ptr<object::Account> getAuthor() const noexcept;
  response::Value getTimestamp() const noexcept;
  response::Value getSignature() const noexcept;
  int getVdf() const noexcept;
  int getTransactionCount() const noexcept;
  std::optional<std::vector<std::shared_ptr<object::Transaction>>> getTransactions() const noexcept;

 private:
  std::shared_ptr<::taraxa::DagBlock> dag_block_;
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::PbftManager> pbft_manager_;
  std::shared_ptr<::taraxa::TransactionManager> transaction_manager_;
  std::function<std::shared_ptr<object::Block>(::taraxa::EthBlockNumber)> get_block_by_num_;

  mutable std::optional<uint64_t> period_;
};

}  // namespace graphql::taraxa