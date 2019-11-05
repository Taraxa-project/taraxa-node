#include "sender_state.hpp"

namespace taraxa::replay_protection::sender_state {

SenderState::SenderState(RLP const& rlp)
    : nonce_max_(rlp[0].toInt<trx_nonce_t>()),
      nonce_watermark_(rlp[1].toInt<trx_nonce_t>()),
      nonce_watermark_exists_(rlp[2].toInt<bool>()) {}

void SenderState::setNonceMax(trx_nonce_t v) {
  if (nonce_max_ < v) {
    nonce_max_ = v;
    nonce_max_dirty_ = true;
  }
}

optional<trx_nonce_t> SenderState::getNonceWatermark() {
  return nonce_watermark_exists_ ? optional(nonce_watermark_) : nullopt;
}

void SenderState::setNonceWatermark(trx_nonce_t v) {
  nonce_watermark_ = v;
  nonce_watermark_exists_ = true;
}

RLPStream SenderState::rlp() {
  return RLPStream(3) << nonce_max_ << nonce_watermark_
                      << nonce_watermark_exists_;
}

}  // namespace taraxa::replay_protection::sender_state
