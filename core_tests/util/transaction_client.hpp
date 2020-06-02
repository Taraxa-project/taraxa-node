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

  Context coinTransfer(addr_t const& to, val_t const& val) const {
    auto final_chain = node_->getFinalChain();
    auto acc = final_chain->get_account(node_->getAddress());
    Context ctx{
        TransactionStage::created,
        Transaction(acc ? acc->Nonce : 0, val, 0, constants::TEST_TX_GAS_LIMIT,
                    bytes(), node_->getSecretKey(), to),
    };
    if (!node_->insertTransaction(ctx.trx, false).first) {
      return ctx;
    }
    ctx.stage = TransactionStage::inserted;
    auto trx_hash = ctx.trx.getHash();
    auto success =
        Wait([&] { return final_chain->isKnownTransaction(trx_hash); },
             opts_.waitUntilExecutedOpts);
    if (success) {
      ctx.stage = TransactionStage::executed;
    }
    return ctx;
  }
};

}  // namespace taraxa::core_tests::util::transaction_client

namespace taraxa::core_tests::util {
using transaction_client::TransactionClient;
}  // namespace taraxa::core_tests::util

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_
