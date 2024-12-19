#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

#include "common/encoding_rlp.hpp"

namespace taraxa {

class PbftVote;
class PillarVote;

/** @addtogroup Vote
 * @{
 */

// TOOD[2865]: move to cpp file
constexpr static size_t kPbftVotesBundleRlpSize{5};

/**
 * @brief Encodes pbft votes into optimized votes bundle rlp
 *
 * @param votes
 * @return votes bundle rlp bytes
 */
dev::bytes encodePbftVotesBundleRlp(const std::vector<std::shared_ptr<PbftVote>>& votes);

/**
 * @brief Decodes pbft votes from optimized votes bundle rlp
 *
 * @param votes_bundle_rlp
 * @return votes
 */
std::vector<std::shared_ptr<PbftVote>> decodePbftVotesBundleRlp(const dev::RLP& votes_bundle_rlp);

struct OptimizedPbftVotesBundle {
  std::vector<std::shared_ptr<PbftVote>> votes;

  HAS_RLP_FIELDS
};

constexpr static size_t kPillarVotesBundleRlpSize{3};

/**
 * @brief Encodes pillar votes into optimized votes bundle rlp
 *
 * @param votes
 * @return votes bundle rlp bytes
 */
dev::bytes encodePillarVotesBundleRlp(const std::vector<std::shared_ptr<PillarVote>>& votes);

/**
 * @brief Decodes pillar votes from optimized votes bundle rlp
 *
 * @param votes_bundle_rlp
 * @return votes
 */
std::vector<std::shared_ptr<PillarVote>> decodePillarVotesBundleRlp(const dev::RLP& votes_bundle_rlp);

struct OptimizedPillarVotesBundle {
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;

  HAS_RLP_FIELDS
};

/** @}*/

}  // namespace taraxa
