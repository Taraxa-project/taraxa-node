#pragma once

#include <libdevcrypto/Common.h>

#include <deque>
#include <optional>
#include <string>

#include "common/types.hpp"
#include "config/config.hpp"
#include "consensus/pbft_chain.hpp"
#include "data.hpp"
#include "util/util.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {

class VoteManager {
 public:
  VoteManager(addr_t node_addr, std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain)
      : final_chain_(final_chain), pbft_chain_(pbft_chain) {
    LOG_OBJECTS_CREATE("VOTE_MGR");
  }
  ~VoteManager() {}

  bool voteValidation(blk_hash_t const& last_pbft_block_hash, Vote const& vote, size_t const valid_sortition_players,
                      size_t const sortition_threshold) const;
  bool addVote(taraxa::Vote const& vote);
  void cleanupVotes(uint64_t pbft_round);
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;
  // for unit test only
  std::vector<Vote> getVotes(uint64_t pbft_round, size_t eligible_voter_count, blk_hash_t last_pbft_block_hash,
                             size_t sortition_threshold);
  std::vector<Vote> getVotes(uint64_t const pbft_round, blk_hash_t const& last_pbft_block_hash,
                             size_t const sortition_threshold, uint64_t eligible_voter_count,
                             std::function<bool(addr_t const&)> const& is_eligible);
  std::string getJsonStr(std::vector<Vote> const& votes);
  std::vector<Vote> getAllVotes();
  bool pbftBlockHasEnoughValidCertVotes(PbftBlockCert const& pbft_block_and_votes, size_t valid_sortition_players,
                                        size_t sortition_threshold, size_t pbft_2t_plus_1) const;

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  // <pbft_round, <vote_hash, vote>>
  std::map<uint64_t, std::map<vote_hash_t, Vote>> unverified_votes_;

  mutable boost::shared_mutex access_;

  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa
