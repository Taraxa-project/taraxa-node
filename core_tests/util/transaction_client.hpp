#ifndef TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_
#define TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_

#include <libdevcrypto/Common.h>

#include <chrono>
#include <thread>

#include "constants.hpp"
#include "full_node.hpp"
#include "types.hpp"
#include "wait.hpp"

namespace taraxa::core_tests::util::transaction_client {
using namespace dev;
using namespace std;
using namespace std::chrono;
using wait::wait;
using wait::WaitOptions;
using wait::WaitOptions_DEFAULT;

struct TransactionClient {
  struct Options {
    WaitOptions waitUntilExecutedOpts;
  };
  enum class TransactionStage {
    created,
    inserted,
    executed,
  };
  struct Context {
    TransactionStage stage;
    Transaction trx;
  };

 private:
  KeyPair key_pair_;
  shared_ptr<FullNode> node_;
  Options opts_;

 public:
  TransactionClient(secret_t const& secret, decltype(node_) const& node,
                    Options const& opts =
                        {
                            {
                                90,
                                nanoseconds(1000 * 1000 * 1000 * 2),
                            },
                        })
      : key_pair_(secret), node_(node), opts_(opts) {}

  Context coinTransfer(string const& to, val_t const& val) const {
    return coinTransfer(addr_t(to), val);
  }

  Context coinTransfer(addr_t const& to, val_t const& val) const {
    auto eth_service = node_->getEthService();
    auto sender_nonce = eth_service->accountNonce(key_pair_.address());
    Context ctx{
        TransactionStage::created,
        Transaction(sender_nonce, val, 0, constants::TEST_TX_GAS_LIMIT, to,
                    bytes(), key_pair_.secret()),
    };
    if (!node_->insertTransaction(ctx.trx)) {
      return ctx;
    }
    ctx.stage = TransactionStage::inserted;
    auto trx_hash = ctx.trx.getHash();
    auto success =
        wait([&] { return eth_service->isKnownTransaction(trx_hash); },
             opts_.waitUntilExecutedOpts);
    if (success) {
      ctx.stage = TransactionStage::executed;
    }
    return ctx;
  }
};

}  // namespace taraxa::core_tests::util::transaction_client

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_
