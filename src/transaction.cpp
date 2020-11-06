#include "transaction.hpp"

#include <libdevcore/CommonJS.h>

#include <string>
#include <utility>

namespace taraxa {
using namespace std;
using namespace dev;

Transaction::Transaction(uint64_t nonce, val_t const &value, val_t const &gas_price, uint64_t gas, bytes data, secret_t const &sk,
                         optional<addr_t> const &receiver, uint64_t chain_id)
    : nonce_(nonce),
      value_(value),
      gas_price_(gas_price),
      gas_(gas),
      data_(move(data)),
      receiver_(receiver),
      chain_id_(chain_id),
      vrs_(sign(sk, hash_for_signature())),
      sender_initialized_(true),
      sender_valid_(vrs_.isValid()),
      sender_(sender_valid_ ? toAddress(sk) : ZeroAddress) {
  getSender();
}

Transaction::Transaction(dev::RLP const &_rlp, bool verify_strict) {
  auto strictness = verify_strict ? dev::RLP::VeryStrict : dev::RLP::LaissezFaire;
  uint fields_processed = 0;
  for (auto const &el : _rlp) {
    ++fields_processed;
    if (fields_processed == 1) {
      nonce_ = el.toInt<uint64_t>(strictness);
    } else if (fields_processed == 2) {
      gas_price_ = el.toInt<dev::u256>(strictness);
    } else if (fields_processed == 3) {
      gas_ = el.toInt<uint64_t>(strictness);
    } else if (fields_processed == 4) {
      if (!el.isEmpty()) {
        receiver_ = el.toHash<dev::Address>(strictness);
      }
    } else if (fields_processed == 5) {
      value_ = el.toInt<dev::u256>(strictness);
    } else if (fields_processed == 6) {
      data_ = el.toBytes(strictness);
    } else if (fields_processed == 7) {
      auto v_ethereum_version = el.toInt<uint64_t>(strictness);
      vrs_.v = ~v_ethereum_version & uint8_t(1);
      if (v_ethereum_version -= (27 + vrs_.v)) {
        if (v_ethereum_version > 8) {
          chain_id_ = (v_ethereum_version - 8) / 2;
        } else {
          BOOST_THROW_EXCEPTION(InvalidChainID());
        }
      }
    } else if (fields_processed == 8) {
      vrs_.r = el.toInt<dev::u256>(strictness);
    } else if (fields_processed == 9) {
      vrs_.s = el.toInt<dev::u256>(strictness);
    } else {
      break;
    }
  }
  if (fields_processed != 9) {
    BOOST_THROW_EXCEPTION(InvalidEncodingSize());
  }
}

trx_hash_t const &Transaction::getHash() const {
  if (!hash_initialized_) {
    hash_initialized_ = true;
    hash_ = dev::sha3(*rlp());
  }
  return hash_;
}

addr_t const &Transaction::get_sender_() const {
  if (!sender_initialized_) {
    sender_initialized_ = true;
    if (auto pubkey = recover(vrs_, hash_for_signature()); pubkey) {
      sender_ = toAddress(pubkey);
      sender_valid_ = true;
    }
  }
  return sender_;
}

addr_t const &Transaction::getSender() const {
  if (auto const &ret = get_sender_(); sender_valid_) {
    return ret;
  }
  throw InvalidSignature("transaction body: " + toJSON().toStyledString() +
                         "\nOriginal RLP: " + (cached_rlp_ ? dev::toJS(*cached_rlp_) : "wasn't created from rlp"));
}

template <bool for_signature>
void Transaction::streamRLP(dev::RLPStream &s) const {
  s.appendList(!for_signature || chain_id_ ? 9 : 6);
  s << nonce_ << gas_price_ << gas_;
  if (receiver_) {
    s << *receiver_;
  } else {
    s << 0;
  }
  s << value_ << data_;
  if (!for_signature) {
    s << vrs_.v + uint64_t(chain_id_ ? (chain_id_ * 2 + 35) : 27) << (u256 const &)vrs_.r  //
      << (u256 const &)vrs_.s;
  } else if (chain_id_) {
    s << chain_id_ << 0 << 0;
  }
}

shared_ptr<bytes> Transaction::rlp(bool cache) const {
  if (cached_rlp_) {
    return cached_rlp_;
  }
  dev::RLPStream s;
  streamRLP<false>(s);
  shared_ptr<bytes> ret(new auto(s.invalidate()));
  return cache ? cached_rlp_ = move(ret) : ret;
};

trx_hash_t Transaction::hash_for_signature() const {
  dev::RLPStream s;
  streamRLP<true>(s);
  return dev::sha3(s.out());
}

Json::Value Transaction::toJSON() const {
  Json::Value res(Json::objectValue);
  res["hash"] = dev::toJS(getHash());
  res["sender"] = dev::toJS(get_sender_());
  res["nonce"] = dev::toJS(getNonce());
  res["value"] = dev::toJS(getValue());
  res["gas_price"] = dev::toJS(getGasPrice());
  res["gas"] = dev::toJS(getGas());
  res["sig"] = dev::toJS((sig_t const &)getVRS());
  res["receiver"] = dev::toJS(getReceiver().value_or(dev::ZeroAddress));
  res["data"] = dev::toJS(getData());
  if (auto v = getChainID(); v) {
    res["chain_id"] = dev::toJS(v);
  }
  return res;
}

}  // namespace taraxa
