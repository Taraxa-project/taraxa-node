#include "transaction.hpp"

#include <string>
#include <utility>

namespace taraxa {
using namespace std;
using namespace dev;

Transaction::Transaction(uint64_t nonce, val_t const &value,
                         val_t const &gas_price, uint64_t gas, bytes data,
                         secret_t const &sk, optional<addr_t> const &receiver,
                         uint64_t chain_id)
    : sender_cached(true),
      nonce_(nonce),
      value_(value),
      gas_price_(gas_price),
      gas_(gas),
      data_(move(data)),
      receiver_(receiver),
      chain_id(chain_id),
      vrs(dev::sign(sk, hash_for_signature())) {
  if (auto pubkey = dev::toPublic(sk); pubkey) {
    sender_ = dev::toAddress(pubkey);
  }
}

Transaction::Transaction(bytes const &_rlp, bool verify_strict) {
  auto strictness =
      verify_strict ? dev::RLP::VeryStrict : dev::RLP::LaissezFaire;
  uint fields_processed = 0;
  for (auto const &el : dev::RLP(_rlp, strictness)) {
    if (fields_processed == 0) {
      nonce_ = el.toInt<uint64_t>(strictness);
    } else if (fields_processed == 1) {
      gas_price_ = el.toInt<dev::u256>(strictness);
    } else if (fields_processed == 2) {
      gas_ = el.toInt<uint64_t>(strictness);
    } else if (fields_processed == 3) {
      if (!el.isEmpty()) {
        receiver_ = el.toHash<dev::Address>(strictness);
      }
    } else if (fields_processed == 4) {
      value_ = el.toInt<dev::u256>(strictness);
    } else if (fields_processed == 5) {
      data_ = el.toBytes(strictness);
    } else if (fields_processed == 6) {
      auto v_ethereum_version = el.toInt<uint64_t>(strictness);
      vrs.v = ~v_ethereum_version & uint8_t(1);
      if ((v_ethereum_version -= (27 + vrs.v)) != 0) {
        if (v_ethereum_version > 8) {
          chain_id = (v_ethereum_version - 8) / 2;
        } else {
          BOOST_THROW_EXCEPTION(InvalidChainID());
        }
      }
    } else if (fields_processed == 7) {
      vrs.r = el.toInt<dev::u256>(strictness);
    } else if (fields_processed == 8) {
      vrs.s = el.toInt<dev::u256>(strictness);
    } else {
      break;
    }
    ++fields_processed;
  }
  if (fields_processed != 9) {
    BOOST_THROW_EXCEPTION(InvalidEncodingSize());
  }
}

trx_hash_t const &Transaction::getHash() const {
  if (!hash_cached) {
    hash_cached = true;
    hash_ = dev::sha3(*rlp());
  }
  return hash_;
}

addr_t const &Transaction::getSender() const {
  if (!sender_cached) {
    sender_cached = true;
    if (auto pubkey = dev::recover(vrs, hash_for_signature()); pubkey) {
      sender_ = toAddress(pubkey);
    }
  }
  return sender_;
}

template <bool for_signature>
void Transaction::streamRLP(dev::RLPStream &s) const {
  s.appendList(!for_signature || chain_id ? 9 : 6);
  s << nonce_ << gas_price_ << gas_;
  if (receiver_) {
    s << *receiver_;
  } else {
    s << 0;
  }
  s << value_ << data_;
  if (!for_signature) {
    s << vrs.v + uint64_t(chain_id ? (chain_id * 2 + 35) : 27)
      << (u256 const &)vrs.r  //
      << (u256 const &)vrs.s;
  } else if (chain_id) {
    s << chain_id << 0 << 0;
  }
}

shared_ptr<bytes> Transaction::rlp(bool cache) const {
  if (cached_rlp) {
    return cached_rlp;
  }
  dev::RLPStream s;
  streamRLP<false>(s);
  shared_ptr<bytes> ret(new auto(s.invalidate()));
  return cache ? cached_rlp = move(ret) : ret;
};

trx_hash_t Transaction::hash_for_signature() const {
  dev::RLPStream s;
  streamRLP<true>(s);
  return dev::sha3(s.out());
}

}  // namespace taraxa
