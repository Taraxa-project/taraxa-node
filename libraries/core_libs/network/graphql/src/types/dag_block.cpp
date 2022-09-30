#include "graphql/types/dag_block.hpp"

#include "graphql/account.hpp"
#include "graphql/transaction.hpp"

namespace graphql::taraxa {

DagBlock::DagBlock(std::shared_ptr<::taraxa::DagBlock> dag_block,
                   std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                   std::shared_ptr<::taraxa::PbftManager> pbft_manager,
                   std::shared_ptr<::taraxa::TransactionManager> transaction_manager) noexcept
    : dag_block_(std::move(dag_block)),
      final_chain_(std::move(final_chain)),
      pbft_manager_(std::move(pbft_manager)),
      transaction_manager_(std::move(transaction_manager)) {}

response::Value DagBlock::getHash() const noexcept { return response::Value(dag_block_->getHash().toString()); }

response::Value DagBlock::getPivot() const noexcept { return response::Value(dag_block_->getPivot().toString()); }

std::vector<response::Value> DagBlock::getTips() const noexcept {
  std::vector<response::Value> tips_result;
  const auto tips = dag_block_->getTips();

  std::transform(tips.begin(), tips.end(), std::back_inserter(tips_result),
                 [](const auto& tip) -> response::Value { return response::Value(tip.toString()); });

  return tips_result;
}

response::Value DagBlock::getLevel() const noexcept {
  return response::Value(static_cast<int>(dag_block_->getLevel()));
}

std::optional<response::Value> DagBlock::getPbftPeriod() const noexcept {
  if (period_) {
    return response::Value(static_cast<int>(*period_));
  }
  const auto [has_period, period] = pbft_manager_->getDagBlockPeriod(::taraxa::blk_hash_t(dag_block_->getHash()));
  if (has_period) {
    period_ = period;
    return {response::Value(static_cast<int>(*period_))};
  }
  return std::nullopt;
}

std::shared_ptr<object::Account> DagBlock::getAuthor() const noexcept {
  if (!period_) {
    const auto [has_period, period] = pbft_manager_->getDagBlockPeriod(::taraxa::blk_hash_t(dag_block_->getHash()));
    if (has_period) {
      period_ = period;
      return std::make_shared<object::Account>(
          std::make_shared<Account>(final_chain_, dag_block_->getSender(), *period_));
    }
  }
  return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, dag_block_->getSender()));
}

response::Value DagBlock::getTimestamp() const noexcept {
  return response::Value(static_cast<int>(dag_block_->getTimestamp()));
}

response::Value DagBlock::getSignature() const noexcept { return response::Value(dev::toJS(dag_block_->getSig())); }

int DagBlock::getVdf() const noexcept { return dag_block_->getDifficulty(); }

int DagBlock::getTransactionCount() const noexcept { return static_cast<int>(dag_block_->getTrxs().size()); }

std::optional<std::vector<std::shared_ptr<object::Transaction>>> DagBlock::getTransactions() const noexcept {
  std::vector<std::shared_ptr<object::Transaction>> transactions_result;
  for (const auto& trx_hash : dag_block_->getTrxs()) {
    transactions_result.push_back(std::make_shared<object::Transaction>(std::make_shared<Transaction>(
        final_chain_, transaction_manager_, transaction_manager_->getTransaction(trx_hash))));
  }

  return transactions_result;
}

}  // namespace graphql::taraxa