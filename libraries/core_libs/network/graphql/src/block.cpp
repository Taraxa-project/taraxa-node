#include "graphql/block.hpp"

#include <cstddef>
#include <optional>

#include "graphql/account.hpp"
#include "graphql/transaction.hpp"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql::taraxa {

Block::Block(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
             std::shared_ptr<::taraxa::TransactionManager> trx_manager,
             std::shared_ptr<const ::taraxa::final_chain::BlockHeader> block_header) noexcept
    : final_chain_(std::move(final_chain)),
      trx_manager_(std::move(trx_manager)),
      block_header_(std::move(block_header)) {}

response::Value Block::getNumber() const noexcept { return response::Value(static_cast<int>(block_header_->number)); }

response::Value Block::getHash() const noexcept { return response::Value(block_header_->hash.toString()); }

std::shared_ptr<object::Block> Block::getParent() const noexcept {
  return std::make_shared<object::Block>(std::make_shared<Block>(
      final_chain_, trx_manager_,
      final_chain_->block_header(final_chain_->block_number(dev::h256(block_header_->parent_hash)))));
}

response::Value Block::getNonce() const noexcept { return response::Value(block_header_->nonce().toString()); }

response::Value Block::getTransactionsRoot() const noexcept {
  return response::Value(block_header_->transactions_root.toString());
}

std::optional<int> Block::getTransactionCount() const noexcept {
  if (!transactions_.size()) {
    return std::optional<int>(final_chain_->transactionCount(block_header_->number));
  } else {
    return std::optional<int>(transactions_.size());
  }
}

response::Value Block::getStateRoot() const noexcept { return response::Value(block_header_->state_root.toString()); }

response::Value Block::getReceiptsRoot() const noexcept {
  return response::Value(block_header_->receipts_root.toString());
}

std::shared_ptr<object::Account> Block::getMiner(std::optional<response::Value>&&) const noexcept {
  return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, block_header_->author));
}

response::Value Block::getExtraData() const noexcept { return response::Value(dev::toHex(block_header_->extra_data)); }

response::Value Block::getGasLimit() const noexcept {
  return response::Value(static_cast<int>(block_header_->gas_limit));
}

response::Value Block::getGasUsed() const noexcept {
  return response::Value(static_cast<int>(block_header_->gas_used));
}

response::Value Block::getTimestamp() const noexcept {
  return response::Value(static_cast<int>(block_header_->timestamp));
}

response::Value Block::getLogsBloom() const noexcept { return response::Value(block_header_->log_bloom.toString()); }

response::Value Block::getMixHash() const noexcept { return response::Value(block_header_->mix_hash().toString()); }

response::Value Block::getDifficulty() const noexcept { return response::Value(block_header_->difficulty().str()); }

response::Value Block::getTotalDifficulty() const noexcept {
  return response::Value(block_header_->difficulty().str());
}

std::optional<int> Block::getOmmerCount() const noexcept { return {}; }

std::optional<std::vector<std::shared_ptr<object::Block>>> Block::getOmmers() const noexcept { return std::nullopt; }

std::shared_ptr<object::Block> Block::getOmmerAt(int&&) const noexcept { return nullptr; }

response::Value Block::getOmmerHash() const noexcept {
  return response::Value(block_header_->uncles_hash().toString());
}

std::optional<std::vector<std::shared_ptr<object::Transaction>>> Block::getTransactions() const noexcept {
  std::vector<std::shared_ptr<object::Transaction>> ret;
  if (!transactions_.size()) {
    transactions_ = final_chain_->transactions(block_header_->number);
    if (!transactions_.size()) return std::nullopt;
  }
  ret.reserve(transactions_.size());
  for (auto& t : transactions_) {
    ret.emplace_back(
        std::make_shared<object::Transaction>(std::make_shared<Transaction>(final_chain_, trx_manager_, t)));
  }
  return ret;
}

std::shared_ptr<object::Transaction> Block::getTransactionAt(response::IntType&& index) const noexcept {
  if (!transactions_.size()) {
    transactions_ = final_chain_->transactions(block_header_->number);
    if (!transactions_.size()) return nullptr;
  }
  if (transactions_.size() < static_cast<size_t>(index)) {
    return nullptr;
  }
  return std::make_shared<object::Transaction>(
      std::make_shared<Transaction>(final_chain_, trx_manager_, transactions_[index]));
}

std::vector<std::shared_ptr<object::Log>> Block::getLogs(BlockFilterCriteria&&) const noexcept {
  std::vector<std::shared_ptr<object::Log>> ret;
  return ret;
}

std::shared_ptr<object::Account> Block::getAccount(response::Value&&) const noexcept {
  return std::make_shared<object::Account>(
      std::make_shared<Account>(final_chain_, block_header_->author, block_header_->number));
}

std::shared_ptr<object::CallResult> Block::getCall(CallData&&) const noexcept { return nullptr; }

response::Value Block::getEstimateGas(CallData&&) const noexcept { return response::Value(0); }

}  // namespace graphql::taraxa