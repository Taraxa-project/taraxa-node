#ifndef TARAXA_NODE_STATEREGISTRY_HPP
#define TARAXA_NODE_STATEREGISTRY_HPP

#include <libdevcore/CommonData.h>
#include <memory>
#include <optional>
#include <utility>
#include "SimpleDBFace.h"
#include "types.hpp"

// TODO: consider append-only interface
namespace taraxa::__StateRegistry__ {
using namespace std;
using namespace dev;

class StateRegistry {
  unique_ptr<SimpleDBFace> db;

 public:
  static inline auto const CURRENT_BLOCK_KEY = toHex(string("_current_block_"));

  explicit StateRegistry(decltype(db) db) : db(move(db)) {}

  void setCurrentBlock(blk_hash_t const &);
  optional<blk_hash_t> getCurrentBlock();
  optional<root_t> getStateRoot(blk_hash_t const &);
  bool putStateRoot(blk_hash_t const &, root_t const &);
};

}  // namespace taraxa::__StateRegistry__

namespace taraxa {
using __StateRegistry__::StateRegistry;
}

#endif  // TARAXA_NODE_STATEREGISTRY_HPP
