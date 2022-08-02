#pragma once

#include <cassert>
#include <memory>

#include "common/types.hpp"
#include "network/network.hpp"
#include "pbft/node_face.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {

const uint64_t kMaxLambda = 60 * 1000;  // in ms, max lambda is 1 minute
#define MAX_STEPS 13                    // Need to be a odd number
#define MAX_WAIT_FOR_SOFT_VOTED_BLOCK_STEPS 20

class Step;

enum StepType { propose = 1, filter, certify, finish, polling };

StepType stepTypeFromId(uint32_t id);

class RoundFace : public std::enable_shared_from_this<RoundFace> {
 public:
  RoundFace(uint64_t id, std::shared_ptr<NodeFace> node)
      : LAMBDA_ms(node->pbft_config_.lambda_ms_min), id_(id), node_(std::move(node)) {}

  virtual ~RoundFace() = default;
  virtual void finish() = 0;

  virtual void setStep(std::unique_ptr<Step> step) = 0;
  virtual uint64_t getStepId() = 0;
  virtual void start(std::optional<uint64_t> step = {}) = 0;
  virtual void run() = 0;
  virtual void sleepUntil(const std::chrono::milliseconds& time) = 0;
  virtual StepType getStepType() const = 0;

  const uint64_t& getId() { return id_; }
  const std::shared_ptr<NodeFace>& getNode() { return node_; }

  const uint64_t& getLambda() { return LAMBDA_ms; }
  const uint64_t& getLambdaBackoff() { return LAMBDA_backoff_multiple; }

  uint64_t msFromStart() {
    auto time_from_round_start = std::chrono::system_clock::now() - start_time_;
    time_from_start_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(time_from_round_start).count();
    return time_from_start_ms_;
  }

  bool next_votes_already_broadcasted_ = false;

  time_point start_time_;
  uint64_t time_from_start_ms_ = 0;

  time_point time_began_waiting_next_voted_block_;
  time_point time_began_waiting_soft_voted_block_;

  bool previous_round_next_voted_null_block_hash_ = false;

  bool executed_pbft_block_ = false;
  bool block_certified_ = false;
  bool next_voted_soft_value_ = false;
  bool next_voted_null_block_hash_ = false;
  bool polling_state_print_log_ = true;

  blk_hash_t previous_round_next_voted_value_ = kNullBlockHash;
  blk_hash_t own_starting_value_for_round_ = kNullBlockHash;

  blk_hash_t last_cert_voted_value_ = kNullBlockHash;

  blk_hash_t soft_voted_block_ = kNullBlockHash;

 protected:
  uint64_t LAMBDA_ms = 0;
  uint64_t LAMBDA_backoff_multiple = 1;

  bool finished_ = false;
  const uint64_t id_;
  std::shared_ptr<NodeFace> node_;
};

}  // namespace taraxa