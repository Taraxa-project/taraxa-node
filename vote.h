#ifndef TARAXA_NODE_VOTE_H
#define TARAXA_NODE_VOTE_H

#include <deque>
#include <optional>
#include <string>
#include "libdevcore/Log.h"
#include "libdevcrypto/Common.h"
#include "pbft_chain.hpp"
#include "types.hpp"
#include "util.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {
class FullNode;
class PbftManager;

struct VrfSortition {
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;
  using vrf_output_t = vrf_wrapper::vrf_output_t;
  using bytes = dev::bytes;
  VrfSortition() = default;
  VrfSortition(vrf_sk_t const& sk, blk_hash_t const& blk, PbftVoteTypes type,
               uint64_t round, size_t step)
      : pk(vrf_wrapper::getVrfPublicKey(sk)),
        blk(blk),
        type(type),
        round(round),
        step(step) {
    std::string msg = blk.toString() + std::to_string(type) + "_" +
                      std::to_string(round) + "_" + std::to_string(step);
    const auto msg_bytes = vrf_wrapper::getRlpBytes(msg);
    proof = vrf_wrapper::getVrfProof(sk, msg_bytes).value();
    output = vrf_wrapper::getVrfOutput(pk, proof, msg_bytes).value();
  }
  VrfSortition(bytes const& rlp);
  bytes getRlpBytes() const;
  bool operator==(VrfSortition const& other) const {
    return pk == other.pk && blk == other.blk && type == other.type && round == other.round &&
           step == other.step && proof == other.proof && output == other.output;
  }
  static inline uint512_t max512bits = std::numeric_limits<uint512_t>::max();
  bool canSpeak(size_t threshold, size_t valid_players) const;
  friend std::ostream& operator<<(std::ostream& strm,
                                  VrfSortition const& vrf_sortition) {
    strm << "[VRF sortition] " << std::endl;
    strm << "  pk: " << vrf_sortition.pk << std::endl;
    strm << "  blk_hash: " << vrf_sortition.blk << std::endl;
    strm << "  type: " << vrf_sortition.type << std::endl;
    strm << "  round: " << vrf_sortition.round << std::endl;
    strm << "  step: " << vrf_sortition.step << std::endl;
    strm << "  proof: " << vrf_sortition.proof << std::endl;
    strm << "  output: " << vrf_sortition.output << std::endl;
    return strm;
  }
  vrf_pk_t pk;
  blk_hash_t blk;
  PbftVoteTypes type;
  uint64_t round;
  size_t step;
  vrf_proof_t proof;
  vrf_output_t output;
};

class Vote {
 public:
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  Vote() = default;
  Vote(public_t const& node_pk, VrfSortition const& vrf_sortition,
       blk_hash_t const& blockhash)
      : node_pk_(node_pk),
        vrf_sortition_(vrf_sortition),
        blockhash_(blockhash) {}

  Vote(public_t const& node_pk, sig_t const& sortition_proof,
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
  sig_t getSortitionProof() const;
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
    strm << "  vrf_pk: " << vote.vrf_pk_ << std::endl;
    strm << "  sortition_proof: " << vote.sortition_proof_ << std::endl;
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
  VrfSortition vrf_sortition_;
  vrf_pk_t vrf_pk_;
  sig_t sortition_proof_;
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
