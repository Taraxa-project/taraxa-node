#include "taraxa_seal_engine.hpp"

namespace taraxa::eth::taraxa_seal_engine {
using dev::eth::SealEngineFactory;
using dev::eth::SealEngineRegistrar;

void TaraxaSealEngine::init() { ETH_REGISTER_SEAL_ENGINE(TaraxaSealEngine); }

void TaraxaSealEngine::verify(Strictness _s, BlockHeader const& _bi,
                              BlockHeader const& _parent,
                              bytesConstRef _block) const {
  dev::eth::SealEngineBase::verify(_s, _bi, _parent, _block);
}

}  // namespace taraxa::eth::taraxa_seal_engine