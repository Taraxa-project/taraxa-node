/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#ifndef VOTE_H
#define VOTE_H

#include "libdevcore/Log.h"
#include "libdevcrypto/Common.h"
#include "pbft_chain.hpp"
#include "types.hpp"
#include "util.hpp"

#include <deque>

namespace taraxa {
class FullNode;
class PbftManager;

class Vote {
 public:
  Vote() = default;
  Vote(public_t const& node_pk, sig_t const& sortition_signature,
       sig_t const& vote_signature, blk_hash_t const& blockhash,
       PbftVoteTypes type, uint64_t round, size_t step);
  Vote(stream& strm);
  ~Vote() {}

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);

  vote_hash_t getHash() const;
  public_t getPublicKey() const;
  sig_t getSortitionSignature() const;
  sig_t getVoteSignature() const;
  blk_hash_t getBlockHash() const;
  PbftVoteTypes getType() const;
  uint64_t getRound() const;
  size_t getStep() const;

 private:
  void streamRLP_(dev::RLPStream& strm) const;

  vote_hash_t vote_hash_;
  public_t node_pk_;
  sig_t sortition_signature_;
  sig_t vote_signatue_;
  blk_hash_t blockhash_;
  PbftVoteTypes type_;
  uint64_t round_;
  size_t step_;
};

class VoteManager {
 public:
  VoteManager() = default;
  ~VoteManager() {}

  void setFullNode(std::shared_ptr<FullNode> node);

  sig_t signVote(secret_t const& node_sk, blk_hash_t const& block_hash,
                 PbftVoteTypes type, uint64_t round, size_t step);
  bool voteValidation(blk_hash_t const& last_pbft_block_hash, Vote const& vote,
                      val_t& account_balance, size_t sortition_threshold) const;

  bool isKnownVote(uint64_t pbft_round, vote_hash_t const& vote_hash) const;

  void addVote(taraxa::Vote const& vote);
  void cleanupVotes(uint64_t pbft_round);
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;
  std::vector<Vote> getVotes(uint64_t pbft_round);
  std::vector<Vote> getVotes(uint64_t pbft_round, bool& sync_peers_pbft_chain);

  std::string getJsonStr(std::vector<Vote>& votes);

  // TODO: TEST functions, will remove later
  std::vector<Vote> getAllVotes();

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  vote_hash_t hash_(std::string const& str) const;

  std::map<uint64_t, std::vector<Vote>> unverified_votes_;

  mutable boost::shared_mutex access_;

  std::weak_ptr<FullNode> node_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;

  mutable dev::Logger log_sil_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "VOTE_MGR")};
  mutable dev::Logger log_err_{
      dev::createLogger(dev::Verbosity::VerbosityError, "VOTE_MGR")};
  mutable dev::Logger log_war_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "VOTE_MGR")};
  mutable dev::Logger log_inf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "VOTE_MGR")};
  mutable dev::Logger log_deb_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "VOTE_MGR")};
  mutable dev::Logger log_tra_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "VOTE_MGR")};
};

}  // namespace taraxa

#endif  // VOTE_H
