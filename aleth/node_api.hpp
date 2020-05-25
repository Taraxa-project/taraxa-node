#ifndef TARAXA_NODE_TARAXA_ALETH_NODE_API_HPP_
#define TARAXA_NODE_TARAXA_ALETH_NODE_API_HPP_

#include <libweb3jsonrpc/Eth.h>

#include <functional>
#include <transaction.hpp>

namespace taraxa::aleth::node_api {
using namespace std;
using namespace dev;
using namespace eth;

class NodeAPI : public virtual rpc::Eth::NodeAPI {
  KeyPair key_pair;
  function<void(::taraxa::Transaction const&)> send_trx;

 public:
  NodeAPI(decltype(key_pair) const& key_pair,
          decltype(send_trx) const& send_trx);

  SyncStatus syncStatus() const override;
  Address address() const override;
  h256 importTransaction(TransactionSkeleton const& _t) override;
  h256 importTransaction(bytes const& rlp) override;
};

}  // namespace taraxa::aleth::node_api

namespace taraxa::aleth {
using node_api::NodeAPI;
}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_TARAXA_ALETH_NODE_API_HPP_
