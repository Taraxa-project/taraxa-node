#ifndef TARAXA_NODE_ETH_UTIL_HPP_
#define TARAXA_NODE_ETH_UTIL_HPP_

#include <libethereum/Transaction.h>

#include "transaction.hpp"

namespace taraxa::eth::util {
using dev::eth::CheckTransaction;
using dev::eth::IncludeSignature;

inline Transaction trx_eth_2_taraxa(EthTransaction const& t) {
  return Transaction(t.rlp(t.hasSignature()
                               ? IncludeSignature::WithSignature
                               : IncludeSignature::WithoutSignature));
}

inline EthTransaction trx_taraxa_2_eth(Transaction const& t) {
  return EthTransaction(t.rlp(t.hasSig()), CheckTransaction::None);
}

}  // namespace taraxa::eth::util

#endif  // TARAXA_NODE_ETH_UTIL_HPP_
