#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

#include "common/types.hpp"

namespace taraxa {

class Vote;
class PbftBlock;
struct Transaction;
class DagBlock;

class SyncBlock {
 public:
  SyncBlock() = default;
  SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<DagBlock>&& ordered_dag_blocks,
            vec_blk_t&& ordered_dag_blocks_hashes, std::vector<Transaction>&& ordered_txs,
            vec_trx_t&& ordered_transactions_hashes);
  SyncBlock(dev::RLP&& all_rlp);
  SyncBlock(bytes const& all_rlp);

  /**** Non thread-safe methods ****/
  // These methods are not thread-safe. It is caller's responsibility to make sure they are not called when using
  // shared object on multiple threads
  void clear();
  void setCertVotes(std::vector<std::shared_ptr<Vote>>&& votes);
  void addCertVote(std::shared_ptr<Vote> vote);
  /**** Non thread-safe methods ****/

  bytes rlp() const;
  void hasEnoughValidCertVotes(size_t valid_sortition_players, size_t sortition_threshold, size_t pbft_2t_plus_1,
                               std::function<size_t(addr_t const&)> const& dpos_eligible_vote_count) const;

  const std::shared_ptr<PbftBlock>& getPbftBlock() const;
  const std::vector<std::shared_ptr<Vote>>& getCertVotes() const;
  const std::vector<DagBlock>& getDagBlocks() const;
  const vec_blk_t& getDagBlocksHashes() const;
  const std::vector<Transaction>& getTransactions() const;
  const vec_trx_t& getTransactionsHashes() const;

  /**
   * @return all unique rewards votes included in ordered_dag_blocks_
   */
  const std::unordered_set<vote_hash_t>& getAllUniqueRewardsVotes() const;

 private:
  std::shared_ptr<PbftBlock> pbft_blk_{nullptr};

  // Dag blocks are ordered based on comparePbftBlockScheduleWithDAGblocks_
  std::vector<DagBlock> ordered_dag_blocks_;

  // ordered_dag_blocks_ transformed into vector of their hashes
  vec_blk_t ordered_dag_blocks_hashes_;

  // Transactions are ordered based on ordered_dag_blocks_
  std::vector<Transaction> ordered_transactions_;

  // ordered_transactions_ transformed into vector of their hashes
  vec_trx_t ordered_transactions_hashes_;

  std::vector<std::shared_ptr<Vote>> cert_votes_;

  // Rewards votes included in dag blocks
  std::unordered_set<vote_hash_t> rewards_votes_;
};

}  // namespace taraxa
