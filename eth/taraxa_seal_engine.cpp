#include "taraxa_seal_engine.hpp"

#include <libethcore/TransactionBase.h>

namespace taraxa::eth::taraxa_seal_engine {
using dev::eth::SealEngineFactory;
using dev::eth::SealEngineRegistrar;

void TaraxaSealEngine::init() { ETH_REGISTER_SEAL_ENGINE(TaraxaSealEngine); }

void TaraxaSealEngine::verify(Strictness _s, BlockHeader const& _bi,
                              BlockHeader const& _parent,
                              bytesConstRef _block) const {
  dev::eth::SealEngineBase::verify(_s, _bi, _parent, _block);
}

void TaraxaSealEngine::verifyTransaction(ImportRequirements::value _ir,
                                         TransactionBase const& _t,
                                         BlockHeader const& _header,
                                         u256 const& _startGasUsed) const {
  if (auto gas = _t.gas(); gas > std::numeric_limits<uint64_t>::max()) {
    throw std::runtime_error("Gas limit is too large: " + gas.str());
  };
  NoProof::verifyTransaction(_ir, _t, _header, _startGasUsed);
}

}  // namespace taraxa::eth::taraxa_seal_engine