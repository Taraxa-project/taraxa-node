#ifndef TARAXA_NODE_VOTE_H
#define TARAXA_NODE_VOTE_H

#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
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
  Vote(bytes const& rlp);
  bool operator==(Vote const& other) const { return rlp() == other.rlp(); }
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
  bytes rlp() const;

  friend std::ostream& operator<<(std::ostream& strm, Vote const& vote) {
    strm << "[Vote] " << std::endl;
    strm << "  vote_hash: " << vote.vote_hash_ << std::endl;
    strm << "  node_pk: " << vote.node_pk_ << std::endl;
    strm << "  sortition_signature: " << vote.sortition_signature_ << std::endl;
    strm << "  vote_signatue: " << vote.vote_signatue_ << std::endl;
    strm << "  blockhash: " << vote.blockhash_ << std::endl;
    strm << "  type: " << vote.type_ << std::endl;
    strm << "  round: " << vote.round_ << std::endl;
    strm << "  step: " << vote.step_ << std::endl;
    return strm;
  }

 private:
  void streamRLP_(dev::RLPStream& strm) const;

  vote_hash_t vote_hash_;  // hash of this vote
  public_t node_pk_;
  sig_t sortition_signature_;
  sig_t vote_signatue_;
  blk_hash_t blockhash_;  // pbft_block_hash
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
                      size_t valid_sortition_players,
                      size_t sortition_threshold) const;

  bool addVote(taraxa::Vote const& vote);
  void cleanupVotes(uint64_t pbft_round);
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;
  std::vector<Vote> getVotes(uint64_t pbft_round,
                             size_t valid_sortiton_players);
  std::vector<Vote> getVotes(uint64_t pbft_round, size_t valid_sortiton_players,
                             bool& sync_peers_pbft_chain);
  std::string getJsonStr(std::vector<Vote>& votes);
  std::vector<Vote> getAllVotes();
  bool pbftBlockHasEnoughValidCertVotes(
      PbftBlockCert const& pbft_block_and_votes, size_t valid_sortition_players,
      size_t sortition_threshold, size_t pbft_2t_plus_1) const;

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  vote_hash_t hash_(std::string const& str) const;
  // <pbft_round, <vote_hash, vote>>
  std::map<uint64_t, std::map<vote_hash_t, Vote>> unverified_votes_;

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
