#include "dag/dag_block_bundle_rlp.hpp"

#include <vector>

#include "common/types.hpp"
#include "dag/dag_block.hpp"

namespace taraxa {

dev::bytes encodeDAGBlocksBundleRlp(const std::vector<DagBlock>& blocks) {
  if (blocks.empty()) {
    return {};
  }

  std::unordered_map<trx_hash_t, uint16_t> trx_hash_map;
  std::vector<trx_hash_t> ordered_trx_hashes;
  std::vector<std::vector<uint16_t>> flat_ranges;  // Flat structure for each block

  for (const auto& block : blocks) {
    std::vector<uint16_t> idx;
    idx.reserve(block.getTrxs().size());

    for (const auto& trx : block.getTrxs()) {
      if (const auto [_, ok] = trx_hash_map.try_emplace(trx, static_cast<uint16_t>(trx_hash_map.size())); ok) {
        ordered_trx_hashes.push_back(trx);
      }
      idx.push_back(trx_hash_map[trx]);
    }

    // Convert indexes into ranges and store in a flat structure
    std::vector<uint16_t> block_flat_ranges;
    uint16_t range_start = idx[0];
    uint16_t range_length = 1;

    for (size_t i = 1; i < idx.size(); ++i) {
      if (idx[i] == range_start + range_length) {
        ++range_length;
      } else {
        block_flat_ranges.push_back(range_start);
        block_flat_ranges.push_back(range_length);
        range_start = idx[i];
        range_length = 1;
      }
    }
    block_flat_ranges.push_back(range_start);
    block_flat_ranges.push_back(range_length);

    flat_ranges.push_back(std::move(block_flat_ranges));
  }

  dev::RLPStream blocks_bundle_rlp(kDAGBlocksBundleRlpSize);
  blocks_bundle_rlp.appendList(ordered_trx_hashes.size());
  for (const auto& trx_hash : ordered_trx_hashes) {
    blocks_bundle_rlp.append(trx_hash);
  }

  blocks_bundle_rlp.appendList(flat_ranges.size());
  for (const auto& block_flat_ranges : flat_ranges) {
    blocks_bundle_rlp.appendList(block_flat_ranges.size());
    for (const auto& range_value : block_flat_ranges) {
      blocks_bundle_rlp.append(range_value);
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

  ordered_trx_hashes.reserve(blocks_bundle_rlp[0].itemCount());
  std::transform(blocks_bundle_rlp[0].begin(), blocks_bundle_rlp[0].end(), std::back_inserter(ordered_trx_hashes),
                 [](const auto& trx_hash_rlp) { return trx_hash_rlp.template toHash<trx_hash_t>(); });

  for (const auto& block_ranges_rlp : blocks_bundle_rlp[1]) {
    std::vector<trx_hash_t> hashes;
    for (size_t i = 0; i < block_ranges_rlp.itemCount(); i += 2) {
      uint16_t start_index = block_ranges_rlp[i].toInt<uint16_t>();
      uint16_t length = block_ranges_rlp[i + 1].toInt<uint16_t>();

      for (uint16_t j = 0; j < length; ++j) {
        hashes.push_back(ordered_trx_hashes[start_index + j]);
      }
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

std::shared_ptr<DagBlock> decodeDAGBlockBundleRlp(uint64_t index, const dev::RLP& blocks_bundle_rlp) {
  if (blocks_bundle_rlp.itemCount() != kDAGBlocksBundleRlpSize) {
    return {};
  }
  if (index >= blocks_bundle_rlp[2].itemCount()) {
    return {};
  }

  std::vector<trx_hash_t> ordered_trx_hashes;
  ordered_trx_hashes.reserve(blocks_bundle_rlp[0].itemCount());
  std::transform(blocks_bundle_rlp[0].begin(), blocks_bundle_rlp[0].end(), std::back_inserter(ordered_trx_hashes),
                 [](const auto& trx_hash_rlp) { return trx_hash_rlp.template toHash<trx_hash_t>(); });

  const auto block_ranges_rlp = blocks_bundle_rlp[1][index];
  std::vector<trx_hash_t> hashes;

  for (size_t i = 0; i < block_ranges_rlp.itemCount(); i += 2) {
    uint16_t start_index = block_ranges_rlp[i].toInt<uint16_t>();
    uint16_t length = block_ranges_rlp[i + 1].toInt<uint16_t>();

    for (uint16_t j = 0; j < length; ++j) {
      hashes.push_back(ordered_trx_hashes[start_index + j]);
    }
  }

  return std::make_shared<DagBlock>(blocks_bundle_rlp[2][index], std::move(hashes));
}

/** @}*/

}  // namespace taraxa
