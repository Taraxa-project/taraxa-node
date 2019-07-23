#include "GenesisState.hpp"

namespace taraxa::_GenesisState_ {

GenesisAccount GenesisAccount::fromPtree(ptree const &p) {
  return {p.get<val_t>("balance")};
}

GenesisState GenesisState::fromPtree(ptree const &p) {
  GenesisState ret{p.get<uint256_t>("account_start_nonce"),
                   p.get_child("block")};
  for (auto &acc : p.get_child("accounts")) {
    ret.accounts[addr_t(acc.first)] = GenesisAccount::fromPtree(acc.second);
  }
  return move(ret);
}

}