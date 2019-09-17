#ifndef TARAXA_NODE_GENESIS_STATE_HPP
#define TARAXA_NODE_GENESIS_STATE_HPP

#include <unordered_map>
#include "boost/property_tree/ptree.hpp"
#include "dag_block.hpp"
#include "types.hpp"

namespace taraxa::_GenesisState_ {
using namespace std;
using boost::property_tree::ptree;

struct GenesisAccount {
  val_t balance;

  static GenesisAccount fromPtree(ptree const &);
};

struct GenesisState {
  uint256_t account_start_nonce;
  DagBlock block;
  unordered_map<addr_t, GenesisAccount> accounts;

  static GenesisState fromPtree(ptree const &);
};

}  // namespace taraxa::_GenesisState_

namespace taraxa {
using _GenesisState_::GenesisAccount;
using _GenesisState_::GenesisState;
}  // namespace taraxa

#endif  // TARAXA_NODE_GENESISSTATE_HPP
