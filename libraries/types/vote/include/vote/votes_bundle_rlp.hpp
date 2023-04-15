#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

namespace taraxa {

class Vote;

/** @addtogroup Vote
 * @{
 */

constexpr static size_t kVotesBundleRlpSize{5};

/**
 * @brief Encodes votes into optimized votes bundle rlp
 *
 * @param votes
 * @param validate_common_data validate if all votes have the same block_hash, period, round and step
 * @return votes bundle rlp bytes
 */
dev::bytes encodeVotesBundleRlp(const std::vector<std::shared_ptr<Vote>>& votes, bool validate_common_data);

/**
 * @brief Decodes votes from optimized votes bundle rlp
 *
 * @param votes_bundle_rlp
 * @return votes
 */
std::vector<std::shared_ptr<Vote>> decodeVotesBundleRlp(const dev::RLP& votes_bundle_rlp);

/** @}*/

}  // namespace taraxa
