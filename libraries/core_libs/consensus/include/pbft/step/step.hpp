#pragma once

#include "pbft/round_face.hpp"

namespace taraxa {

class Step {
 public:
  Step(uint64_t id, std::shared_ptr<RoundFace> round);
  virtual ~Step() = default;
  virtual void run() = 0;
  virtual bool finished() const { return finished_; }
  uint64_t getId() const { return id_; }
  StepType getType() const { return type_; }

 protected:
  virtual void init() = 0;
  /**
   * @brief Terminate the next voting value of the PBFT block hash
   * @return true if terminate the vote value successfully
   */
  bool giveUpNextVotedBlock_();

  /**
   * @brief Place a vote, save it in the verified votes queue, and gossip to peers
   * @param blockhash vote on PBFT block hash
   * @param vote_type vote type
   * @param round PBFT round
   * @return vote weight
   */
  size_t placeVote_(blk_hash_t const& blockhash, PbftVoteTypes vote_type, uint64_t round);

  /**
   * @brief Terminate the soft voting value of the PBFT block hash
   * @return true if terminate the vote value successfully
   */
  bool giveUpSoftVotedBlock_();

  virtual void finish_() { finished_ = true; }
  bool finished_ = false;

  const uint64_t id_;
  const StepType type_;
  std::shared_ptr<RoundFace> round_;
  std::shared_ptr<NodeFace> node_;

  LOG_OBJECTS_REF_DEFINE
};
}  // namespace taraxa