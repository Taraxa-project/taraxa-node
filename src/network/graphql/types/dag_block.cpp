#include "dag_block.hpp"

#include "network/graphql/account.hpp"
#include "network/graphql/transaction.hpp"

namespace graphql::taraxa {

DagBlock::DagBlock(const std::shared_ptr<::taraxa::DagBlock>& dag_block,
                   const std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                   const std::shared_ptr<::taraxa::PbftManager>& pbft_manager,
                   const std::shared_ptr<::taraxa::TransactionManager>& transaction_manager)
    : dag_block_(dag_block),
      final_chain_(final_chain),
      pbft_manager_(pbft_manager),
      transaction_manager_(transaction_manager) {}

service::FieldResult<response::Value> DagBlock::getHash(service::FieldParams&& params) const {
  return response::Value(dag_block_->getHash().toString());
}

service::FieldResult<response::Value> DagBlock::getPivot(service::FieldParams&& params) const {
  return response::Value(dag_block_->getPivot().toString());
}

service::FieldResult<std::vector<response::Value>> DagBlock::getTips(service::FieldParams&& params) const {
  std::vector<response::Value> tips_result;
  const auto tips = dag_block_->getTips();

  std::transform(tips.begin(), tips.end(), std::back_inserter(tips_result),
                 [](const auto& tip) -> response::Value { return response::Value(tip.toString()); });

  return tips_result;
}

service::FieldResult<response::Value> DagBlock::getLevel(service::FieldParams&& params) const {
  return response::Value(static_cast<response::IntType>(dag_block_->getLevel()));
}

service::FieldResult<std::optional<response::Value>> DagBlock::getPbftPeriod(service::FieldParams&& params) const {
  auto period = pbft_manager_->getDagBlockPeriod(::taraxa::blk_hash_t(dag_block_->getHash()));

  // TODO: casting uint64_t to int -> should be string used instead ?
  return response::Value(static_cast<response::IntType>(period.first ? period.second : -1));
}

service::FieldResult<std::shared_ptr<object::Account>> DagBlock::getAuthor(service::FieldParams&& params) const {
  return std::make_shared<Account>(final_chain_, dag_block_->getSender());
}

service::FieldResult<response::Value> DagBlock::getTimestamp(service::FieldParams&& params) const {
  // TODO: casting uint64_t to int -> should be string used instead ?
  return response::Value(static_cast<response::IntType>(dag_block_->getTimestamp()));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<object::Transaction>>>> DagBlock::getTransactions(
    service::FieldParams&& params) const {
  std::vector<std::shared_ptr<object::Transaction>> transactions_result;
  const auto transactions = dag_block_->getTrxs();

  for (const auto& trx_hash : dag_block_->getTrxs()) {
    // TODO: which form to choose ?
    // 1.
    //    const auto trx =
    //    std::make_shared<dev::eth::Transaction>(transaction_manager_->getTransaction(trx_hash)->second,
    //    dev::eth::CheckTransaction::None,
    //                                                             true, trx_hash);
    // 2.
    const auto trx = std::make_shared<dev::eth::Transaction>(transaction_manager_->getTransaction(trx_hash)->second);

    transactions_result.push_back(std::make_shared<Transaction>(final_chain_, trx));
  }

  return transactions_result;
}

}  // namespace graphql::taraxa
