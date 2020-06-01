#include "node_api.hpp"

#include <optional>

namespace taraxa::aleth {
using namespace std;
using namespace dev;
using namespace eth;
using namespace rpc;

struct NodeAPIImpl : virtual Eth::NodeAPI {
  KeyPair key_pair;
  function<void(::taraxa::Transaction const&)> send_trx;

  NodeAPIImpl(decltype(key_pair) key_pair, decltype(send_trx) send_trx)
      : key_pair(key_pair), send_trx(send_trx) {}

  SyncStatus syncStatus() const override { return {}; }

  Address address() const override { return key_pair.address(); }

  h256 importTransaction(TransactionSkeleton const& t) override {
    ::taraxa::Transaction trx(
        t.nonce.value_or(0), t.value, t.gasPrice.value_or(0), t.gas.value_or(0),
        t.data, key_pair.secret(), t.to ? optional(t.to) : std::nullopt);
    send_trx(trx);
    return trx.getHash();
  }

  h256 importTransaction(bytes const& rlp) override {
    ::taraxa::Transaction trx(rlp);
    send_trx(trx);
    return trx.getHash();
  }
};

unique_ptr<Eth::NodeAPI> NewNodeAPI(
    KeyPair key_pair, function<void(::taraxa::Transaction const&)> send_trx) {
  return u_ptr(new NodeAPIImpl(move(key_pair), move(send_trx)));
}

}  // namespace taraxa::aleth
