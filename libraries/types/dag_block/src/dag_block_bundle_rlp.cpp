#include "dag/dag_block_bundle_rlp.hpp"

#include <vector>

#include "common/types.hpp"
#include "dag/dag_block.hpp"

namespace taraxa {

dev::bytes encodeDAGBlocksBundleRlp(const std::vector<DagBlock>& blocks) {
  if (blocks.empty()) {
    return {};
  }

  std::unordered_map<trx_hash_t, uint16_t> trx_hash_map;  // Map to store transaction hash and its index
  std::vector<trx_hash_t> ordered_trx_hashes;
  std::vector<std::vector<uint16_t>> indexes;

  for (const auto& block : blocks) {
    std::vector<uint16_t> idx;
    idx.reserve(block.getTrxs().size());

    for (const auto& trx : block.getTrxs()) {
      if (trx_hash_map.find(trx) == trx_hash_map.end()) {
        trx_hash_map[trx] = static_cast<uint16_t>(trx_hash_map.size());
        ordered_trx_hashes.push_back(trx);  // Track the insertion order
      }
      idx.push_back(trx_hash_map[trx]);
    }
    indexes.push_back(idx);
  }

  dev::RLPStream blocks_bundle_rlp(kDAGBlocksBundleRlpSize);
  blocks_bundle_rlp.appendList(ordered_trx_hashes.size());
  for (const auto& trx_hash : ordered_trx_hashes) {
    blocks_bundle_rlp.append(trx_hash);
  }
  blocks_bundle_rlp.appendList(indexes.size());
  for (const auto& idx : indexes) {
    blocks_bundle_rlp.appendList(idx.size());
    for (const auto& i : idx) {
      blocks_bundle_rlp.append(i);
    }
  }
  blocks_bundle_rlp.appendList(blocks.size());
  for (const auto& block : blocks) {
    blocks_bundle_rlp.appendRaw(block.rlp(true, false));
  }
  return blocks_bundle_rlp.invalidate();
}

std::vector<DagBlock> decodeDAGBlocksBundleRlp(const dev::RLP& blocks_bundle_rlp) {
  if (blocks_bundle_rlp.itemCount() != kDAGBlocksBundleRlpSize) {
    return {};
  }

  std::vector<trx_hash_t> ordered_trx_hashes;
  std::vector<std::vector<trx_hash_t>> dags_trx_hashes;

  // Decode transaction hashes and
  ordered_trx_hashes.reserve(blocks_bundle_rlp[0].itemCount());
  for (const auto& trx_hash_rlp : blocks_bundle_rlp[0]) {
    ordered_trx_hashes.push_back(trx_hash_rlp.toHash<trx_hash_t>());
  }

  for (const auto& idx_rlp : blocks_bundle_rlp[1]) {
    std::vector<trx_hash_t> hashes;
    hashes.reserve(idx_rlp.itemCount());
    for (const auto& i : idx_rlp) {
      hashes.push_back(ordered_trx_hashes[i.toInt<uint16_t>()]);
    }
    dags_trx_hashes.push_back(std::move(hashes));
  }

  std::vector<DagBlock> blocks;
  blocks.reserve(blocks_bundle_rlp[2].itemCount());

  for (size_t i = 0; i < blocks_bundle_rlp[2].itemCount(); i++) {
    auto block = DagBlock(blocks_bundle_rlp[2][i], std::move(dags_trx_hashes[i]));
    blocks.push_back(std::move(block));
  }

  return blocks;
}

// // constexpr static size_t kPillarblocksBundleRlpSize{3};

// /**
//  * @brief Decodes single dag block from optimized blocks bundle rlp
//  *
//  * @param blocks_bundle_rlp
//  * @return block
//  */
// std::shared_ptr<DagBlock> decodeDAGBlockBundleRlp(uint64_t index, const dev::RLP& blocks_bundle_rlp);

/** @}*/

}  // namespace taraxa
