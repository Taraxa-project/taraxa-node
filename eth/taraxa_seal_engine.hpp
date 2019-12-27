#ifndef TARAXA_NODE_ETH_TARAXA_SEAL_ENGINE_HPP_
#define TARAXA_NODE_ETH_TARAXA_SEAL_ENGINE_HPP_

#include <libethcore/SealEngine.h>

namespace taraxa::eth::taraxa_seal_engine {
using dev::bytesConstRef;
using dev::Invalid256;
using dev::StringHashMap;
using dev::u256;
using dev::eth::BlockHeader;
using dev::eth::ChainOperationParams;
using dev::eth::ImportRequirements;
using dev::eth::NoProof;
using dev::eth::Strictness;
using dev::eth::TransactionBase;
using std::string;

struct TaraxaSealEngine : NoProof {
  static string name() { return "TaraxaSealEngine"; }
  static void init();
  void verify(Strictness _s, BlockHeader const& _bi, BlockHeader const& _parent,
              bytesConstRef _block) const override;
  void verifyTransaction(ImportRequirements::value _ir,
                         const TransactionBase& _t, const BlockHeader& _header,
                         const u256& _startGasUsed) const override;
};

}  // namespace taraxa::eth::taraxa_seal_engine

#endif  // TARAXA_NODE_ETH_TARAXA_SEAL_ENGINE_HPP_
