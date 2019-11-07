#ifndef TARAXA_NODE_ACCOUNT_STATE_STATE_SNAPSHOT_HPP_
#define TARAXA_NODE_ACCOUNT_STATE_STATE_SNAPSHOT_HPP_

#include <libdevcore/RLP.h>
#include "types.hpp"

namespace taraxa::account_state::state_snapshot {
using dev::RLP;
using dev::RLPStream;

struct StateSnapshot {
  dag_blk_num_t block_number;
  blk_hash_t block_hash;
  root_t state_root;

  static StateSnapshot make(RLP const &rlp);
  RLPStream rlp() const;
  bool operator==(StateSnapshot const &s) const;
  bool operator!=(StateSnapshot const &s) const { return !operator==(s); }
};

};  // namespace taraxa::account_state::state_snapshot

#endif  // TARAXA_NODE_ACCOUNT_STATE_STATE_SNAPSHOT_HPP_
