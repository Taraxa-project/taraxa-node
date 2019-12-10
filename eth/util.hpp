#ifndef TARAXA_NODE_ETH_UTIL_HPP_
#define TARAXA_NODE_ETH_UTIL_HPP_

#include <libethereum/Transaction.h>

#include "transaction.hpp"

namespace taraxa::eth::util {
using TaraxaTrx = ::taraxa::Transaction;
using EthTrx = ::dev::eth::Transaction;
using dev::eth::CheckTransaction;
using dev::eth::IncludeSignature;

inline TaraxaTrx trx_eth_2_taraxa(EthTrx const& t) {
  return TaraxaTrx(t.rlp(t.hasSignature()
                             ? IncludeSignature::WithSignature
                             : IncludeSignature::WithoutSignature));
}

inline EthTrx trx_taraxa_2_eth(TaraxaTrx const& t) {
  return EthTrx(t.rlp(t.hasSig()), CheckTransaction::None);
}

}  // namespace taraxa::eth::util

#endif  // TARAXA_NODE_ETH_UTIL_HPP_
