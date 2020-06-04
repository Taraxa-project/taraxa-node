#ifndef TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_
#define TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_

#include <libdevcrypto/Common.h>

#include <atomic>
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
  shared_ptr<FullNode> node_;
  Options opts_;

 public:
  TransactionClient(decltype(node_) const& node,
                    Options const& opts =
                        {
                            {
                                90,
                                nanoseconds(1000 * 1000 * 1000 * 2),
                            },
                        })
      : node_(node), opts_(opts) {}

  Context coinTransfer(addr_t const& to, val_t const& val,
                       optional<KeyPair> const& from_k = {},
                       bool verify_executed = true) const {
    auto final_chain = node_->getFinalChain();
    // As long as nonce rules are completely disabled, this hack allows to
    // generate unique nonces that contribute to transaction uniqueness.
    // Without this, it's very possible in these tests to have hash collisions,
    // if you just use a constant value like 0 or even get the nonce from the
    // account state. The latter won't work in general because in some tests
    // we don't wait for previous transactions for a sender to complete before
    // sending a new one
    static atomic<uint64_t> nonce = 100000;
    Context ctx{
        TransactionStage::created,
        Transaction(++nonce, val, 0, constants::TEST_TX_GAS_LIMIT, bytes(),
                    from_k ? from_k->secret() : node_->getSecretKey(), to),
    };
    if (!node_->insertTransaction(ctx.trx, false).first) {
      return ctx;
    }
    ctx.stage = TransactionStage::inserted;
    auto trx_hash = ctx.trx.getHash();
    if (verify_executed) {
      auto success =
          Wait([&] { return final_chain->isKnownTransaction(trx_hash); },
               opts_.waitUntilExecutedOpts);
      if (success) {
        ctx.stage = TransactionStage::executed;
      }
    }
    return ctx;
  }
};

}  // namespace taraxa::core_tests::util::transaction_client

namespace taraxa::core_tests::util {
using transaction_client::TransactionClient;
}  // namespace taraxa::core_tests::util

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_
