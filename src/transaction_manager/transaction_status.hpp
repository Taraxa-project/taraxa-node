#pragma once

namespace taraxa {

enum class TransactionStatusEnum {
  invalid = 0,
  in_block,  // confirmed state, inside of block created by us or someone else
  in_queue_unverified,
  in_queue_verified,
  executed,
  not_seen
};

class TransactionStatus {
 public:
  TransactionStatusEnum status = TransactionStatusEnum::not_seen;
  uint64_t period = 0;
  uint64_t position = 0;

  TransactionStatus() = default;

  TransactionStatus(TransactionStatusEnum transactionStatus, uint64_t tPeriod = 0, uint64_t tPosition = 0) {
    status = transactionStatus;
    period = tPeriod;
    position = tPosition;
  }

  TransactionStatus(dev::RLP const &rlp) {
    if (!rlp.isList()) {
      throw std::invalid_argument("TransactionStatus RLP must be a list");
    }
    auto it = rlp.begin();
    status = (TransactionStatusEnum)(*it++).toInt<uint16_t>();
    period = (*it++).toInt<uint64_t>();
    position = (*it++).toInt<uint64_t>();
  }

  dev::bytes rlp() const {
    dev::RLPStream s;
    s.appendList(3);
    s << (uint16_t)status << period << position;
    return s.invalidate();
  }
};

}  // namespace taraxa
