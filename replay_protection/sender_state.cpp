#include "sender_state.hpp"

namespace taraxa::replay_protection::sender_state {

void SenderState::setNonceMax(trx_nonce_t v) {
  if (nonce_max_ < v) {
    nonce_max_ = v;
    nonce_max_dirty_ = true;
  }
}

}  // namespace taraxa::replay_protection::sender_state
