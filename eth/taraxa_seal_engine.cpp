#include "taraxa_seal_engine.hpp"

#include <libethcore/TransactionBase.h>

#include "conf/chain_config.hpp"

namespace taraxa::eth::taraxa_seal_engine {
using conf::chain_config::ChainConfig;
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
  if (_ir & ImportRequirements::TransactionSignatures) {
    auto& params = chainParams();
    static auto const default_chain_id = ChainConfig::DEFAULT().eth.chainID;
    // TODO remove the default_chain_id check,
    // and start using real chain id in transactions
    if (default_chain_id != params.chainID) {
      if (_header.number() >= params.EIP158ForkBlock) {
        _t.checkChainId(params.chainID);
      } else {
        _t.checkChainId();
      }
    }
  }
  NoProof::verifyTransaction(_ir, _t, _header, _startGasUsed);
}

}  // namespace taraxa::eth::taraxa_seal_engine