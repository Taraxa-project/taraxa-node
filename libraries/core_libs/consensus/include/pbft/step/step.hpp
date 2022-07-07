#pragma once
#include <cassert>
#include <memory>

#include "common/types.hpp"
#include "network/network.hpp"

namespace taraxa {

using time_point = std::chrono::system_clock::time_point;

enum PbftStates { propose = 1, filter, certify, finish, polling };

// TODO: Should be removed or minimized
struct CommonState {
  PbftStates state_ = propose;

  time_point round_clock_initial_datetime_;
  time_point now_;

  time_point time_began_waiting_next_voted_block_;
  time_point time_began_waiting_soft_voted_block_;
  std::chrono::duration<double> duration_;
  uint64_t next_step_time_ms_ = 0;
  uint64_t elapsed_time_in_round_ms_ = 0;

  blk_hash_t previous_round_next_voted_value_ = kNullBlockHash;
  blk_hash_t own_starting_value_for_round_ = kNullBlockHash;

  blk_hash_t last_soft_voted_value_ = kNullBlockHash;
  blk_hash_t last_cert_voted_value_ = kNullBlockHash;

  blk_hash_t soft_voted_block_for_this_round_ = kNullBlockHash;
};

// TODO: Should be real node interface?
struct NodeFace {
  NodeFace(const addr_t& addr, const secret_t& node_sk, const vrf_wrapper::vrf_sk_t& vrf_sk,
           std::shared_ptr<DbStorage> db, std::shared_ptr<NextVotesManager> nvm, std::shared_ptr<PbftChain> pc,
           std::shared_ptr<VoteManager> vm, std::shared_ptr<DagManager> dm, std::shared_ptr<DagBlockManager> dbm,
           std::shared_ptr<TransactionManager> tm, std::shared_ptr<FinalChain> fc)
      : node_addr_(addr),
        node_sk_(node_sk),
        vrf_sk_(vrf_sk),
        db_(std::move(db)),
        next_votes_manager_(std::move(nvm)),
        pbft_chain_(std::move(pc)),
        vote_mgr_(std::move(vm)),
        dag_mgr_(std::move(dm)),
        dag_blk_mgr_(std::move(dbm)),
        trx_mgr_(std::move(tm)),
        final_chain_(std::move(fc)) {}
  // TODO:  circular dependency. should be resolved
  std::weak_ptr<PbftManager> pbft_manager_;
  addr_t node_addr_;
  secret_t node_sk_;
  vrf_wrapper::vrf_sk_t vrf_sk_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<NextVotesManager> next_votes_manager_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
};

class Step {
 public:
  Step(uint64_t id, std::shared_ptr<CommonState> state, std::shared_ptr<NodeFace> node)
      : id_(id), state_(std::move(state)), node_(std::move(node)) {
    const auto& node_addr = node_->node_addr_;
    LOG_OBJECTS_CREATE("STEP" + std::to_string(id_));
  }
  virtual ~Step() = default;
  virtual void run() = 0;
  virtual bool finished() const { return finished_; }
  uint64_t getId() { return id_; }

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

  virtual void finish_() { finished_ = true; }
  bool finished_ = false;

  uint64_t id_;
  std::shared_ptr<CommonState> state_;
  std::shared_ptr<NodeFace> node_;

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa