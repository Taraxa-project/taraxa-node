#include "node_api.hpp"

#include <optional>

#include "util/util.hpp"

namespace taraxa::aleth {
using namespace std;
using namespace dev;
using namespace eth;
using namespace rpc;

struct NodeAPIImpl : virtual Eth::NodeAPI {
  uint64_t chain_id_ = 0;
  KeyPair key_pair;
  function<void(::taraxa::Transaction const&)> send_trx;

  NodeAPIImpl(decltype(chain_id_) chain_id, decltype(key_pair) key_pair, decltype(send_trx) send_trx)
      : chain_id_(chain_id), key_pair(move(key_pair)), send_trx(move(send_trx)) {}

  uint64_t chain_id() const override { return chain_id_; }

  SyncStatus syncStatus() const override { return {}; }

  Address const& address() const override { return key_pair.address(); }

  h256 importTransaction(TransactionSkeleton const& t) override {
    ::taraxa::Transaction trx(t.nonce.value_or(0), t.value, t.gasPrice.value_or(0), t.gas.value_or(0), t.data,
                              key_pair.secret(), t.to ? optional(t.to) : std::nullopt, chain_id_);
    trx.rlp(true);
    send_trx(trx);
    return trx.getHash();
  }

  h256 importTransaction(bytes&& rlp) override {
    ::taraxa::Transaction trx(move(rlp), true);
    send_trx(trx);
    return trx.getHash();
  }
};

unique_ptr<Eth::NodeAPI> NewNodeAPI(uint64_t chain_id, KeyPair key_pair,
                                    function<void(::taraxa::Transaction const&)> send_trx) {
  return std::make_unique<NodeAPIImpl>(chain_id, move(key_pair), move(send_trx));
}

}  // namespace taraxa::aleth
