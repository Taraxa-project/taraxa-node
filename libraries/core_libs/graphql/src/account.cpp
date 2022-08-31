#include "graphql/account.hpp"

#include "libdevcore/CommonJS.h"

using namespace std::literals;

namespace graphql::taraxa {

Account::Account(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::Address address) noexcept
    : kAddress(std::move(address)), final_chain_(std::move(final_chain)) {
  account_ = final_chain->get_account(kAddress, final_chain->last_block_number());
}

response::Value Account::getAddress() const noexcept { return response::Value(kAddress.toString()); }

response::Value Account::getBalance() const noexcept {
  if (account_) {
    return response::Value(dev::toJS(account_->balance));
  }
  return response::Value(0);
}

response::Value Account::getTransactionCount() const noexcept {
  if (account_) {
    return response::Value(dev::toJS(account_->nonce));
  }
  return response::Value(0);
}

response::Value Account::getCode() const noexcept {
  return response::Value(dev::toJS(final_chain_->get_code(kAddress, final_chain_->last_block_number())));
}

response::Value Account::getStorage(response::Value&& slotArg) const noexcept {
  return response::Value(dev::toJS(final_chain_->get_account_storage(kAddress, dev::u256(slotArg.get<std::string>()))));
}

}  // namespace graphql::taraxa