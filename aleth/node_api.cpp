#include "node_api.hpp"

#include <gmock/internal/gmock-pp.h>

#include <optional>

#define DECL(p) decltype(p) p
#define INIT(p) p(move(p))

#define CTOR_0(T, p1) T()
#define CTOR_1(T, p1) T(DECL(p1)) : INIT(p1)
#define CTOR_2(T, p1, p2) T(DECL(p1), DECL(p2)) : INIT(p1), INIT(p2)
#define CTOR_3(T, p1, p2, p3) \
  T(DECL(p1), DECL(p2), DECL(p3)) : INIT(p1), INIT(p2), INIT(p3)
#define CTOR_4(T, p1, p2, p3, p4)           \
  T(DECL(p1), DECL(p2), DECL(p3), DECL(p4)) \
      : INIT(p1), INIT(p2), INIT(p3), INIT(p4)
#define CTOR_5(T, p1, p2, p3, p4, p5)                 \
  T(DECL(p1), DECL(p2), DECL(p3), DECL(p4), DECL(p5)) \
      : INIT(p1), INIT(p2), INIT(p3), INIT(p4), INIT(p5)
#define CTOR_6(T, p1, p2, p3, p4, p5, p6)                       \
  T(DECL(p1), DECL(p2), DECL(p3), DECL(p4), DECL(p5), DECL(p6)) \
      : INIT(p1), INIT(p2), INIT(p3), INIT(p4), INIT(p5), INIT(p6)
#define CTOR_7(T, p1, p2, p3, p4, p5, p6, p7)                             \
  T(DECL(p1), DECL(p2), DECL(p3), DECL(p4), DECL(p5), DECL(p6), DECL(p7)) \
      : INIT(p1), INIT(p2), INIT(p3), INIT(p4), INIT(p5), INIT(p6), INIT(p7)

#define CTOR(T, ...) \
  GMOCK_PP_CAT(CTOR_, GMOCK_PP_NARG(__VA_ARGS__))(T, __VA_ARGS__)

namespace taraxa::aleth {
using namespace std;
using namespace dev;
using namespace eth;
using namespace rpc;

struct NodeAPIImpl : virtual Eth::NodeAPI {
  KeyPair key_pair;
  function<void(::taraxa::Transaction const&)> send_trx;

  CTOR(NodeAPIImpl, key_pair, send_trx) {}

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
