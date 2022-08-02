#pragma once

#include "common/types.hpp"
#include "network/network.hpp"

namespace taraxa {

using time_point = std::chrono::system_clock::time_point;

class PbftManager;

// TODO: Should be real node interface?
struct NodeFace {
  NodeFace(const addr_t& addr, const secret_t& node_sk, const vrf_wrapper::vrf_sk_t& vrf_sk,
           const PbftConfig& pbft_config, std::shared_ptr<DbStorage> db, std::shared_ptr<NextVotesManager> nvm,
           std::shared_ptr<PbftChain> pc, std::shared_ptr<VoteManager> vm, std::shared_ptr<DagManager> dm,
           std::shared_ptr<DagBlockManager> dbm, std::shared_ptr<TransactionManager> tm, std::shared_ptr<FinalChain> fc)
      : node_addr_(addr),
        node_sk_(node_sk),
        vrf_sk_(vrf_sk),
        pbft_config_(pbft_config),
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
  const PbftConfig& pbft_config_;
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

}  // namespace taraxa