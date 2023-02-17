#include "graphql/transaction.hpp"

#include <optional>

#include "graphql/account.hpp"
#include "graphql/block.hpp"
#include "graphql/log.hpp"
#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql::taraxa {

Transaction::Transaction(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                         std::shared_ptr<::taraxa::TransactionManager> trx_manager,
                         std::shared_ptr<::taraxa::Transaction> transaction) noexcept
    : final_chain_(std::move(final_chain)),
      trx_manager_(std::move(trx_manager)),
      transaction_(std::move(transaction)) {}

response::Value Transaction::getHash() const noexcept { return response::Value(transaction_->getHash().toString()); }

response::Value Transaction::getNonce() const noexcept { return response::Value(transaction_->getNonce().str()); }

std::optional<int> Transaction::getIndex() const noexcept {
  if (!location_) {
    location_ = final_chain_->transaction_location(transaction_->getHash());
    if (!location_) return std::nullopt;
  }
  return {location_->index};
}

std::shared_ptr<object::Account> Transaction::getFrom(std::optional<response::Value>&&) const {
  if (!location_) {
    location_ = final_chain_->transaction_location(transaction_->getHash());
    if (!location_) {
      return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, transaction_->getSender()));
    }
  }
  return std::make_shared<object::Account>(
      std::make_shared<Account>(final_chain_, transaction_->getSender(), location_->blk_n));
}

std::shared_ptr<object::Account> Transaction::getTo(std::optional<response::Value>&&) const {
  if (!transaction_->getReceiver()) return nullptr;
  if (!location_) {
    location_ = final_chain_->transaction_location(transaction_->getHash());
    if (!location_) {
      return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, *transaction_->getReceiver()));
    }
  }
  return std::make_shared<object::Account>(
      std::make_shared<Account>(final_chain_, *transaction_->getReceiver(), location_->blk_n));
}

response::Value Transaction::getValue() const noexcept { return response::Value(transaction_->getValue().str()); }

response::Value Transaction::getGasPrice() const noexcept { return response::Value(transaction_->getGasPrice().str()); }

response::Value Transaction::getGas() const noexcept {
  return response::Value(static_cast<int>(transaction_->getGas()));
}

response::Value Transaction::getInputData() const noexcept {
  return response::Value(dev::toJS(transaction_->getData()));
}

std::shared_ptr<object::Block> Transaction::getBlock() const {
  if (!location_) {
    location_ = final_chain_->transaction_location(transaction_->getHash());
    if (!location_) return nullptr;
  }
  return std::make_shared<object::Block>(
      std::make_shared<Block>(final_chain_, trx_manager_, final_chain_->block_header(location_->blk_n)));
}

std::optional<response::Value> Transaction::getStatus() const noexcept {
  if (!receipt_) {
    receipt_ = final_chain_->transaction_receipt(transaction_->getHash());
    if (!receipt_) return std::nullopt;
  }
  return response::Value(static_cast<int>(receipt_->status_code));
}

std::optional<response::Value> Transaction::getGasUsed() const noexcept {
  if (!receipt_) {
    receipt_ = final_chain_->transaction_receipt(transaction_->getHash());
    if (!receipt_) return std::nullopt;
  }
  return response::Value(static_cast<int>(receipt_->gas_used));
}

std::optional<response::Value> Transaction::getCumulativeGasUsed() const noexcept {
  if (!receipt_) {
    receipt_ = final_chain_->transaction_receipt(transaction_->getHash());
    if (!receipt_) return std::nullopt;
  }
  return response::Value(static_cast<int>(receipt_->cumulative_gas_used));
}

std::shared_ptr<object::Account> Transaction::getCreatedContract(std::optional<response::Value>&&) const noexcept {
  if (!receipt_) {
    receipt_ = final_chain_->transaction_receipt(transaction_->getHash());
    if (!receipt_) return nullptr;
  }
  if (!receipt_->new_contract_address) return nullptr;
  return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, *receipt_->new_contract_address));
}

std::optional<std::vector<std::shared_ptr<object::Log>>> Transaction::getLogs() const noexcept {
  std::vector<std::shared_ptr<object::Log>> logs;
  if (!receipt_) {
    receipt_ = final_chain_->transaction_receipt(transaction_->getHash());
    if (!receipt_) return std::nullopt;
  }

  for (int i = 0; i < static_cast<int>(receipt_->logs.size()); ++i) {
    logs.push_back(std::make_shared<object::Log>(
        std::make_shared<Log>(final_chain_, trx_manager_, shared_from_this(), receipt_->logs[i], i)));
  }

  return logs;
}

response::Value Transaction::getR() const noexcept { return response::Value(dev::toJS(transaction_->getVRS().r)); }

response::Value Transaction::getS() const noexcept { return response::Value(dev::toJS(transaction_->getVRS().s)); }

response::Value Transaction::getV() const noexcept { return response::Value(dev::toJS(transaction_->getVRS().v)); }

}  // namespace graphql::taraxa