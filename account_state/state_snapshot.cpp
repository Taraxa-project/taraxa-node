#include "state_snapshot.hpp"

namespace taraxa::account_state::state_snapshot {

StateSnapshot StateSnapshot::make(RLP const& rlp) {
  return {
      rlp[0].toInt<decltype(StateSnapshot::block_number)>(),
      rlp[1].toHash<decltype(StateSnapshot::block_hash)>(),
      rlp[2].toHash<decltype(StateSnapshot::state_root)>(),
  };
}

RLPStream StateSnapshot::rlp() const {
  return RLPStream(3) << block_number << block_hash << state_root;
}

bool StateSnapshot::operator==(StateSnapshot const& s) const {
  return block_number == s.block_number && block_hash == s.block_hash &&
         state_root == s.state_root;
}

};