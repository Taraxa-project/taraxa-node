#include "transaction/system_transaction.hpp"

#include "common/constants.hpp"
#include "common/encoding_rlp.hpp"

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

SystemTransaction::SystemTransaction(const bytes &_bytes, bool verify_strict) {
  dev::RLP rlp;
  try {
    rlp = dev::RLP(_bytes);
  } catch (const dev::RLPException &e) {
    // TODO[1881]: this should be removed when we will add typed transactions support
    std::string error_msg =
        "Can't parse transaction from RLP. Use legacy transactions because typed transactions aren't supported yet.";
    error_msg += "\nException details:\n";
    error_msg += e.what();
    BOOST_THROW_EXCEPTION(dev::RLPException() << dev::errinfo_comment(error_msg));
  }

  fromRLP(rlp, verify_strict);
  sender_ = kTaraxaSystemAccount;
}

SystemTransaction::SystemTransaction(const dev::RLP &_rlp, bool verify_strict) {
  fromRLP(_rlp, verify_strict);
  sender_ = kTaraxaSystemAccount;
}

const addr_t &SystemTransaction::getSender() const { return sender_; }

void SystemTransaction::streamRLP(dev::RLPStream &s, bool) const {
  // always serialize as for the signature
  s.appendList(9);
  s << nonce_ << gas_price_ << gas_;
  if (receiver_) {
    s << *receiver_;
  } else {
    s << 0;
  }
  s << value_ << data_;
  s << chain_id_ << 0 << 0;
}

void SystemTransaction::fromRLP(const dev::RLP &_rlp, bool verify_strict) {
  util::rlp_tuple(util::RLPDecoderRef(_rlp, verify_strict), nonce_, gas_price_, gas_, receiver_, value_, data_, vrs_.v,
                  vrs_.r, vrs_.s);
}

}  // namespace taraxa