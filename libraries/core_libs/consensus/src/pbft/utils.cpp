#include "pbft/utils.hpp"

namespace taraxa::pbft {

blk_hash_t calculateOrderHash(const std::vector<blk_hash_t> &dag_block_hashes,
                              const std::vector<trx_hash_t> &trx_hashes) {
  if (dag_block_hashes.empty()) {
    return kNullBlockHash;
  }
  dev::RLPStream order_stream(2);
  order_stream.appendList(dag_block_hashes.size());
  for (auto const &blk_hash : dag_block_hashes) {
    order_stream << blk_hash;
  }
  order_stream.appendList(trx_hashes.size());
  for (auto const &trx_hash : trx_hashes) {
    order_stream << trx_hash;
  }
  return dev::sha3(order_stream.out());
}

blk_hash_t calculateOrderHash(const std::vector<DagBlock> &dag_blocks, const SharedTransactions &trxs) {
  if (dag_blocks.empty()) {
    return kNullBlockHash;
  }
  dev::RLPStream order_stream(2);
  order_stream.appendList(dag_blocks.size());
  for (auto const &blk : dag_blocks) {
    order_stream << blk.getHash();
  }
  order_stream.appendList(trxs.size());
  for (auto const &trx : trxs) {
    order_stream << trx->getHash();
  }
  return dev::sha3(order_stream.out());
}
}  // namespace taraxa::pbft