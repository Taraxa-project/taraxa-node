#include "transaction.hpp"

#include <string>
#include <utility>

namespace taraxa {

Transaction::Transaction(stream &strm) { deserialize(strm); }

Transaction::Transaction(string const &json) {
  try {
    Json::Value root;
    Json::Reader reader;
    bool parsing_successful = reader.parse(json, root);
    if (!parsing_successful) {
      std::cerr << "Parsing Json Transaction failed" << json << std::endl;
      assert(false);
    }

    hash_ = trx_hash_t(root["hash"].asString());
    type_ = toEnum<Transaction::Type>(root["type"].asUInt());
    nonce_ = dev::jsToInt(root["nonce"].asString());
    value_ = val_t(root["value"].asString());
    gas_price_ = val_t(root["gas_price"].asString());
    gas_ = dev::jsToInt(root["gas"].asString());
    sig_ = sig_t(root["sig"].asString());
    receiver_ = addr_t(root["receiver"].asString());
    string data = root["data"].asString();
    data_ = dev::jsToBytes(data);
    if (!sig_.isZero()) {
      dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
      if (sig_struct.isValid()) {
        vrs_ = sig_struct;
      }
    }
    chain_id_ = root["chain_id"].asInt();
    ;
    cached_sender_ = addr_t(root["hash"].asString());
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    assert(false);
  }
}

Transaction::Transaction(bytes const &_rlp) {
  dev::RLP const rlp(_rlp);
  if (!rlp.isList())
    throw std::invalid_argument("transaction RLP must be a list");

  nonce_ = rlp[0].toInt<uint64_t>();
  gas_price_ = rlp[1].toInt<dev::u256>();
  gas_ = rlp[2].toInt<uint64_t>();
  type_ = rlp[3].isEmpty() ? taraxa::Transaction::Type::Create
                           : taraxa::Transaction::Type::Call;
  receiver_ = rlp[3].isEmpty()
                  ? dev::Address()
                  : rlp[3].toHash<dev::Address>(dev::RLP::VeryStrict);
  value_ = rlp[4].toInt<dev::u256>();

  if (!rlp[5].isData())
    throw std::invalid_argument("transaction data RLP must be an array");

  data_ = rlp[5].toBytes();

  int const v = rlp[6].toInt<int>();
  dev::h256 const r = rlp[7].toInt<dev::u256>();
  dev::h256 const s = rlp[8].toInt<dev::u256>();

  if (!r && !s) {
    chain_id_ = v;
    sig_ = dev::SignatureStruct{r, s, 0};
  } else {
    if (v > 36)
      chain_id_ = (v - 35) / 2;
    else if (v == 27 || v == 28)
      chain_id_ = -4;
    else
      throw std::invalid_argument("InvalidSignature()");

    sig_ = dev::SignatureStruct{r, s,
                                static_cast<::byte>(v - (chain_id_ * 2 + 35))};
  }
  dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
  if (sig_struct.isValid()) {
    vrs_ = sig_struct;
  }

  if (rlp.itemCount() > 9)
    throw std::invalid_argument("too many fields in the transaction RLP");
  updateHash();
}

bool Transaction::serialize(stream &strm) const {
  bool ok = true;
  ok &= write(strm, hash_);
  ok &= write(strm, type_);
  ok &= write(strm, nonce_);
  ok &= write(strm, value_);
  ok &= write(strm, gas_price_);
  ok &= write(strm, gas_);
  ok &= write(strm, receiver_);
  ok &= write(strm, sig_);
  std::size_t byte_size = data_.size();
  ok &= write(strm, byte_size);
  for (auto i = 0; i < byte_size; ++i) {
    ok &= write(strm, data_[i]);
  }
  assert(ok);
  return ok;
}

bool Transaction::deserialize(stream &strm) {
  bool ok = true;
  ok &= read(strm, hash_);
  ok &= read(strm, type_);
  ok &= read(strm, nonce_);
  ok &= read(strm, value_);
  ok &= read(strm, gas_price_);
  ok &= read(strm, gas_);
  ok &= read(strm, receiver_);
  ok &= read(strm, sig_);
  std::size_t byte_size;
  ok &= read(strm, byte_size);
  data_.resize(byte_size);
  for (auto i = 0; i < byte_size; ++i) {
    ok &= read(strm, data_[i]);
  }
  if (!sig_.isZero()) {
    dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
    if (sig_struct.isValid()) {
      vrs_ = sig_struct;
    }
  }

  assert(ok);
  return ok;
}

Json::Value Transaction::getJson() const {
  Json::Value res;
  res["hash"] = dev::toJS(hash_);
  res["type"] = (uint)type_;
  res["nonce"] = dev::toJS(nonce_);
  res["value"] = dev::toJS(value_);
  res["gas_price"] = dev::toJS(gas_price_);
  res["gas"] = dev::toJS(gas_);
  res["sig"] = dev::toJS(sig_);
  res["receiver"] = dev::toJS(receiver_);
  res["data"] = dev::toJS(data_);
  res["chain_id"] = chain_id_;
  res["sender"] = dev::toJS(cached_sender_);
  return res;
}

string Transaction::getJsonStr() const {
  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, getJson());
}

void Transaction::sign(secret_t const &sk) {
  if (!sig_) {
    sig_ = dev::sign(sk, sha3(false));
    dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
    if (sig_struct.isValid()) {
      vrs_ = sig_struct;
    }
  }
  sender();
  updateHash();
}

bool Transaction::verifySig() const {
  if (!sig_) return false;
  auto msg = sha3(false);
  auto pk = dev::recover(sig_, msg);
  // offload send() computation to verifier
  sender();
  return dev::verify(pk, sig_, msg);
}

addr_t const &Transaction::sender() const {
  if (!cached_sender_) {
    if (!sig_) {
      return cached_sender_;
    }
    auto p = dev::recover(sig_, sha3(false));
    assert(p);
    cached_sender_ =
        dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
  }
  return cached_sender_;
}

void Transaction::streamRLP(dev::RLPStream &s, bool include_sig,
                            bool _forEip155hash) const {
  if (type_ == Transaction::Type::Null) return;
  s.appendList(include_sig || _forEip155hash ? 9 : 6);
  s << nonce_ << gas_price_ << gas_;
  if (type_ == Transaction::Type::Call) {
    s << receiver_;
  } else {
    s << "";
  }
  s << value_ << data_;
  if (include_sig) {
    assert(vrs_);
    if (hasZeroSig()) {
      s << chain_id_;
    } else {
      int const v_offset = chain_id_ * 2 + 35;
      s << (vrs_->v + v_offset);
    }
    s << (dev::u256)vrs_->r << (dev::u256)vrs_->s;
  } else if (_forEip155hash)
    s << chain_id_ << 0 << 0;
}

bytes Transaction::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig, chain_id_ > 0 && !include_sig);
  return s.out();
};

blk_hash_t Transaction::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

}  // namespace taraxa
