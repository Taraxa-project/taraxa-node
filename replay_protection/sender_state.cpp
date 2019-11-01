#include "sender_state.hpp"

namespace taraxa::replay_protection::sender_state {

bool SenderState::setNonceMax(trx_nonce_t v) {
  if (nonce_max_ >= v) {
    return false;
  }
  nonce_max_ = v;
  dirty_ = nonce_max_dirty_ = true;
  return true;
}

void SenderState::setNonceWatermark(trx_nonce_t v) {
  if (nonce_watermark_ == v && nonce_watermark_exists_) {
    return;
  }
  nonce_watermark_ = v;
  dirty_ = nonce_watermark_exists_ = true;
}

}  // namespace taraxa::replay_protection::sender_state
