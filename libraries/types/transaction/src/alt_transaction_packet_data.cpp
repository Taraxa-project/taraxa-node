#include "transaction/alt_transaction_packet_data.hpp"

namespace taraxa {

RLP_FIELDS_DEFINE(TransactionPacketData::SingleTransaction, nonce_, gas_price_, gas_, receiver_, value_, data_, v, r, s)

}  // namespace taraxa
