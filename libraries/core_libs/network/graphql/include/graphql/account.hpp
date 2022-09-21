#pragma once

#include <memory>
#include <string>

#include "AccountObject.h"
#include "final_chain/final_chain.hpp"
#include "final_chain/state_api.hpp"

namespace graphql::taraxa {

class Account {
 public:
  explicit Account(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::Address address,
                   ::taraxa::EthBlockNumber blk_n) noexcept;
  explicit Account(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain, dev::Address address) noexcept;

  response::Value getAddress() const noexcept;
  response::Value getBalance() const noexcept;
  response::Value getTransactionCount() const noexcept;
  response::Value getCode() const noexcept;
  response::Value getStorage(response::Value&& slotArg) const;

 private:
  const dev::Address kAddress;
  std::optional<::taraxa::state_api::Account> account_;
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
};
}  // namespace graphql::taraxa