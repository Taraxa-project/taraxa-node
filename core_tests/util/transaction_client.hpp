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
                                60,
                                nanoseconds(1000 * 1000 * 1000 * 2),
                            },
                        })
      : key_pair_(secret), node_(node), opts_(opts) {}

  Context coinTransfer(string const& to, val_t const& val) const {
    return coinTransfer(addr_t(to), val);
  }

  Context coinTransfer(addr_t const& to, val_t const& val) const {
    auto state_registry = node_->getStateRegistry();
    auto initial_state = state_registry->getCurrentState();
    auto nonce = initial_state.getNonce(key_pair_.address());
    Context ctx{
        TransactionStage::created,
        Transaction(nonce, val, 0, constants::TEST_TX_GAS_LIMIT, to, bytes(),
                    key_pair_.secret()),
    };
    if (!node_->insertTransaction(ctx.trx)) {
      return ctx;
    }
    ctx.stage = TransactionStage::inserted;
    auto trx_hash = ctx.trx.getHash();
    auto lastSeenBlkNum = initial_state.getSnapshot().block_number;
    auto success = wait(
        [&] {
          auto curr_block_num =
              state_registry->getCurrentSnapshot().block_number;
          while (lastSeenBlkNum < curr_block_num) {
            ++lastSeenBlkNum;
            auto snapshot = state_registry->getSnapshot(lastSeenBlkNum);
            auto blk = node_->getDagBlock(snapshot->block_hash);
            for (auto const& hash : blk->getTrxs()) {
              if (trx_hash == hash) {
                ctx.stage = TransactionStage::executed;
                return true;
              }
            }
          }
          return false;
        },
        opts_.waitUntilExecutedOpts);
    return ctx;
  }
};

}  // namespace taraxa::core_tests::util::transaction_client

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_TRANSACTION_CLIENT_HPP_
