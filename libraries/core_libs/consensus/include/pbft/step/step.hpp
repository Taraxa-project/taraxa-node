#pragma once

#include "pbft/round_face.hpp"

namespace taraxa {

/**
 * @ingroup PBFT
 * @brief Step class
 */
class Step {
 public:
  Step(uint64_t id, const std::chrono::milliseconds& finish_time, std::shared_ptr<RoundFace> round);
  virtual ~Step() = default;
  virtual void run() = 0;
  virtual bool finished() const { return finished_; }
  uint64_t getId() const { return kId_; }
  StepType getType() const { return kType_; }

 protected:
  /**
   * @brief Terminate the next voting value of the PBFT block hash
   * @return true if terminate the vote value successfully
   */
  bool giveUpNextVotedBlock();

  /**
   * @brief Detects vote type from id and returns it
   * @return vote type
   */
  PbftVoteType getVoteType() const;

  /**
   * @brief Place a vote, save it in the verified votes queue, and gossip to peers
   * @param blockhash vote on PBFT block hash
   * @param round PBFT round
   * @return vote weight
   */
  size_t placeVote(blk_hash_t const& blockhash, uint64_t round);

  /**
   * @brief Terminate the soft voting value of the PBFT block hash
   * @return true if terminate the vote value successfully
   */
  bool giveUpSoftVotedBlock();

  virtual void finish() {
    finished_ = true;
    round_->sleepUntil(kFinishTime_);
  }
  bool finished_ = false;

  const uint64_t kId_;
  const StepType kType_;
  const std::chrono::milliseconds kFinishTime_;
  std::shared_ptr<RoundFace> round_;
  std::shared_ptr<NodeFace> node_;

  LOG_OBJECTS_REF_DEFINE
};
}  // namespace taraxa