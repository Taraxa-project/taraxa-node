#include "transaction/system_transaction.hpp"

#include "common/constants.hpp"

namespace taraxa {
SystemTransaction::SystemTransaction(const trx_nonce_t &nonce, const val_t &value, const val_t &gas_price, gas_t gas,
                                     bytes data, const std::optional<addr_t> &receiver, uint64_t chain_id) {
  nonce_ = nonce;
  value_ = value;
  gas_price_ = gas_price;
  gas_ = gas;
  data_ = std::move(data);
  receiver_ = receiver;
  chain_id_ = chain_id;
  sender_ = kTaraxaSystemAccount;
}

const addr_t &SystemTransaction::getSender() const { return sender_; }

}  // namespace taraxa