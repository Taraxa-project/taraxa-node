#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

namespace taraxa {

class DagBlock;

/** @addtogroup DAG
 * @{
 */

constexpr static size_t kDAGBlocksBundleRlpSize{3};

/**
 * @brief Encodes pbft blocks into optimized blocks bundle rlp
 *
 * @param blocks
 * @return blocks bundle rlp bytes
 */
dev::bytes encodeDAGBlocksBundleRlp(const std::vector<DagBlock>& blocks);

/**
 * @brief Decodes pbft blocks from optimized blocks bundle rlp
 *
 * @param blocks_bundle_rlp
 * @return blocks
 */
std::vector<DagBlock> decodeDAGBlocksBundleRlp(const dev::RLP& blocks_bundle_rlp);

/**
 * @brief Decodes single dag block from optimized blocks bundle rlp
 *
 * @param blocks_bundle_rlp
 * @return block
 */
std::shared_ptr<DagBlock> decodeDAGBlockBundleRlp(uint64_t index, const dev::RLP& blocks_bundle_rlp);

/** @}*/

}  // namespace taraxa
