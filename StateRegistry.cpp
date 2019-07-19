#include "StateRegistry.hpp"

namespace taraxa::__StateRegistry__ {

void StateRegistry::setCurrentBlock(blk_hash_t const& blk_hash) {
  db->update(CURRENT_BLOCK_KEY, blk_hash.hex());
  db->commit();
}

optional<blk_hash_t> StateRegistry::getCurrentBlock() {
  auto const& last_block_hash_str = db->get(CURRENT_BLOCK_KEY);
  return last_block_hash_str.empty()
             ? nullopt
             : optional(blk_hash_t(last_block_hash_str));
}

optional<root_t> StateRegistry::getStateRoot(blk_hash_t const& blk_hash) {
  auto const& root_str = db->get(blk_hash.hex());
  return root_str.empty() ? nullopt : optional(root_t(root_str));
}

bool StateRegistry::putStateRoot(blk_hash_t const& blk_hash,
                                     root_t const& root) {
  if (db->put(blk_hash.hex(), root.hex())) {
    db->commit();
    return true;
  }
  return false;
}

}  // namespace taraxa::__StateRootRegistry__