#include "transaction_manager/transaction.hpp"

#include <libdevcore/CommonJS.h>

#include <string>
#include <utility>

#include "common/encoding_rlp.hpp"

namespace taraxa {
using namespace std;
using namespace dev;

uint64_t toChainID(u256 const &val) {
  if (val == 0 || std::numeric_limits<uint64_t>::max() < val) {
    BOOST_THROW_EXCEPTION(Transaction::InvalidSignature("eip-155 chain id must be in the open interval: (0, 2^64)"));
  }
  return static_cast<uint64_t>(val);
}

Transaction::Transaction(uint64_t nonce, val_t const &value, val_t const &gas_price, uint64_t gas, bytes data,
                         secret_t const &sk, optional<addr_t> const &receiver, uint64_t chain_id)
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

Transaction::Transaction(dev::RLP const &_rlp, bool verify_strict, h256 const &hash, bool rlp_w_sender)
    : hash_(hash), hash_initialized_(!hash.isZero()) {
  u256 v, r, s;

  if (_rlp.itemCount() < 10) {
    util::rlp_tuple(util::RLPDecoderRef(_rlp, verify_strict), nonce_, gas_price_, gas_, receiver_, value_, data_, v, r,
                    s);
  } else {
    addr_t sender;
    util::rlp_tuple(util::RLPDecoderRef(_rlp, verify_strict), nonce_, gas_price_, gas_, receiver_, value_, data_, v, r,
                    s, sender);

    if (rlp_w_sender && !sender.isZero()) {
      sender_initialized_ = true;
      sender_valid_ = true;
      sender_ = sender;
    }
  }

  // the following piece of code should be equivalent to
  // https://github.com/ethereum/aleth/blob/644d420f8ac0f550a28e9ca65fe1ad1905d0f069/libethcore/TransactionBase.cpp#L58-L78
  // Plus, we don't allow `0` for chain_id. In this code, `chain_id_ == 0` means there is no chain id at all.
  if (!r && !s) {
    chain_id_ = toChainID(v);
  } else {
    if (36 < v) {
      chain_id_ = toChainID((v - 35) / 2);
    } else if (v != 27 && v != 28) {
      BOOST_THROW_EXCEPTION(InvalidSignature(
          "only values 27 and 28 are allowed for non-replay protected transactions for the 'v' signature field"));
    }
    vrs_.v = chain_id_ ? byte{v - (u256{chain_id_} * 2 + 35)} : byte{v - 27};
    vrs_.r = r;
    vrs_.s = s;
  }
}

trx_hash_t const &Transaction::getHash() const {
  if (!hash_initialized_) {
    std::unique_lock l(hash_mu_.val, std::try_to_lock);
    if (!l.owns_lock()) {
      l.lock();
      return hash_;
    }
    hash_ = dev::sha3(*rlp());
    hash_initialized_ = true;
  }
  return hash_;
}

addr_t const &Transaction::get_sender_() const {
  if (!sender_initialized_) {
    std::unique_lock l(sender_mu_.val, std::try_to_lock);
    if (!l.owns_lock()) {
      l.lock();
      return sender_;
    }
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

template <bool for_signature, bool w_sender>
void Transaction::streamRLP(dev::RLPStream &s) const {
  s.appendList(!for_signature || chain_id_ ? ((w_sender) ? 10 : 9) : 6);
  s << nonce_ << gas_price_ << gas_;
  if (receiver_) {
    s << *receiver_;
  } else {
    s << 0;
  }
  s << value_ << data_;
  if (!for_signature) {
    s << vrs_.v + uint64_t(chain_id_ ? (chain_id_ * 2 + 35) : 27) << (u256 const &)vrs_.r << (u256 const &)vrs_.s;
    if (w_sender) {
      assert(sender_valid_);
      s << sender_;
    }
  } else if (chain_id_) {
    s << chain_id_ << 0 << 0;
  }
}

shared_ptr<bytes> Transaction::rlp(bool w_sender) const {
  if (cached_rlp_ && !w_sender) {
    return cached_rlp_;
  }
  std::unique_lock l(cached_rlp_mu_.val, std::try_to_lock);
  if (!l.owns_lock()) {
    l.lock();
    return cached_rlp_;
  }
  dev::RLPStream s;
  if (w_sender)
    streamRLP<false, true>(s);
  else
    streamRLP<false, false>(s);
  return cached_rlp_ = make_shared<bytes>(move(s.invalidate()));
}

trx_hash_t Transaction::hash_for_signature() const {
  dev::RLPStream s;
  streamRLP<true, false>(s);
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
  if (auto v = getChainID()) {
    res["chain_id"] = dev::toJS(v);
  }
  return res;
}

}  // namespace taraxa
