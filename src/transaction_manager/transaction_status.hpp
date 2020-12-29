#ifndef TARAXA_NODE__TRANSACTION_STATUS_HPP_
#define TARAXA_NODE__TRANSACTION_STATUS_HPP_

namespace taraxa {

enum class TransactionStatus {
  invalid = 0,
  in_block,  // confirmed state, inside of block created by us or someone else
  in_queue_unverified,
  in_queue_verified,
  not_seen
};

}

#endif  // TARAXA_NODE__TRANSACTION_STATUS_HPP_
