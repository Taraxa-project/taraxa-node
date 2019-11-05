#ifndef TARAXA_NODE_REPLAY_PROTECTION_SENDER_STATE_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_SENDER_STATE_HPP_

#include <libdevcore/RLP.h>
#include <optional>
#include "types.hpp"

namespace taraxa::replay_protection::sender_state {
using dev::RLP;
using dev::RLPStream;
using std::nullopt;
using std::optional;

class SenderState {
  // persistent
  trx_nonce_t nonce_max_ = 0;
  trx_nonce_t nonce_watermark_ = 0;
  bool nonce_watermark_exists_ = 0;
  // transient
  bool nonce_max_dirty_ = 0;
  bool default_initialized_ = 0;

 public:
  SenderState() : default_initialized_(true) {}

  explicit SenderState(RLP const& rlp)
      : nonce_max_(rlp[0].toInt<trx_nonce_t>()),
        nonce_watermark_(rlp[1].toInt<trx_nonce_t>()),
        nonce_watermark_exists_(rlp[2].toInt<bool>()) {}

  auto isDefaultInitialized() { return default_initialized_; };

  auto isNonceMaxDirty() { return nonce_max_dirty_; }

  auto getNonceMax() { return nonce_max_; }

  void setNonceMax(trx_nonce_t v);

  optional<trx_nonce_t> getNonceWatermark() {
    return nonce_watermark_exists_ ? optional(nonce_watermark_) : nullopt;
  };

  void setNonceWatermark(trx_nonce_t v) {
    nonce_watermark_ = v;
    nonce_watermark_exists_ = true;
  }

  RLPStream rlp() {
    return RLPStream(3) << nonce_max_ << nonce_watermark_
                        << nonce_watermark_exists_;
  }
};

}  // namespace taraxa::replay_protection::sender_state

#endif  // TARAXA_NODE_REPLAY_PROTECTION_SENDER_STATE_HPP_
