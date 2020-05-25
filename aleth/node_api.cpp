#include "node_api.hpp"

#include <optional>

namespace taraxa::aleth::node_api {

NodeAPI::NodeAPI(decltype(key_pair) const& key_pair,
                 decltype(send_trx) const& send_trx)
    : key_pair(key_pair), send_trx(send_trx) {}

SyncStatus NodeAPI::syncStatus() const { return {}; }

Address NodeAPI::address() const { return key_pair.address(); }

h256 NodeAPI::importTransaction(TransactionSkeleton const& t) {
  ::taraxa::Transaction trx(t.nonce, t.value, t.gasPrice, t.gas, t.data,
                            key_pair.secret(),
                            t.to ? optional(t.to) : std::nullopt);
  send_trx(trx);
  return trx.getHash();
}

h256 NodeAPI::importTransaction(bytes const& rlp) {
  ::taraxa::Transaction trx(rlp);
  send_trx(trx);
  return trx.getHash();
}

}  // namespace taraxa::aleth::node_api
