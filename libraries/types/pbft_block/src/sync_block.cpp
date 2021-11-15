#include "pbft/sync_block.hpp"

#include <libdevcore/CommonJS.h>

#include "dag/dag_block.hpp"
#include "pbft/pbft_block.hpp"
#include "transaction/transaction.hpp"
#include "vote/vote.hpp"

namespace taraxa {

using namespace std;

SyncBlock::SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<DagBlock>&& ordered_dag_blocks,
                     vec_blk_t&& ordered_dag_blocks_hashes, std::vector<Transaction>&& ordered_txs,
                     vec_trx_t&& ordered_transactions_hashes)
    : pbft_blk_(std::move(pbft_blk)),
      ordered_dag_blocks_(std::move(ordered_dag_blocks)),
      ordered_dag_blocks_hashes_(ordered_dag_blocks_hashes),
      ordered_transactions_(std::move(ordered_txs)),
      ordered_transactions_hashes_(std::move(ordered_transactions_hashes)) {}

SyncBlock::SyncBlock(dev::RLP&& rlp) {
  auto it = rlp.begin();
  pbft_blk_ = std::make_shared<PbftBlock>(*it++);
  for (auto const vote_rlp : *it++) {
    cert_votes_.push_back(std::make_shared<Vote>(vote_rlp));
  }

  for (auto const dag_block_rlp : *it++) {
    const auto dag_block = DagBlock(dag_block_rlp);

    ordered_dag_blocks_hashes_.push_back(dag_block.getHash());
    ordered_dag_blocks_.push_back(std::move(dag_block));
  }

  for (auto const trx_rlp : *it) {
    const auto tx = Transaction(trx_rlp);

    ordered_transactions_hashes_.push_back(tx.getHash());
    ordered_transactions_.push_back(std::move(tx));
  }
}

SyncBlock::SyncBlock(bytes const& all_rlp) : SyncBlock(dev::RLP(all_rlp)) {}

bytes SyncBlock::rlp() const {
  dev::RLPStream s(4);
  s.appendRaw(pbft_blk_->rlp(true));
  s.appendList(cert_votes_.size());
  for (auto const& v : cert_votes_) {
    s.appendRaw(v->rlp(true));
  }
  s.appendList(ordered_dag_blocks_.size());
  for (auto const& b : ordered_dag_blocks_) {
    s.appendRaw(b.rlp(true));
  }
  s.appendList(ordered_transactions_.size());
  for (auto const& t : ordered_transactions_) {
    s.appendRaw(*t.rlp());
  }
  return s.out();
}

void SyncBlock::clear() {
  pbft_blk_.reset();
  cert_votes_.clear();

  ordered_dag_blocks_.clear();
  ordered_dag_blocks_hashes_.clear();

  ordered_transactions_.clear();
  ordered_transactions_hashes_.clear();
}

void SyncBlock::setCertVotes(std::vector<std::shared_ptr<Vote>>&& votes) { cert_votes_ = std::move(votes); }

const std::shared_ptr<PbftBlock>& SyncBlock::getPbftBlock() const { return pbft_blk_; }

const std::vector<std::shared_ptr<Vote>>& SyncBlock::getCertVotes() const { return cert_votes_; }

const std::vector<DagBlock>& SyncBlock::getDagBlocks() const { return ordered_dag_blocks_; }

const vec_blk_t& SyncBlock::getDagBlocksHashes() const { return ordered_dag_blocks_hashes_; }

const std::vector<Transaction>& SyncBlock::getTransactions() const { return ordered_transactions_; }

const vec_trx_t& SyncBlock::getTransactionsHashes() const { return ordered_transactions_hashes_; }

void SyncBlock::hasEnoughValidCertVotes(size_t dpos_total_votes_count, size_t sortition_threshold,
                                        size_t pbft_2t_plus_1) const {
  if (cert_votes_.empty()) {
    throw std::logic_error("No cert votes provided! The synced PBFT block comes from a malicious player.");
  }

  if (cert_votes_.size() != pbft_2t_plus_1) {
    std::stringstream err;
    err << "PBFT block " << pbft_blk_->getBlockHash() << " has a wrong number of cert votes. There are "
        << cert_votes_.size() << " cert votes with the block. But 2t+1 is " << pbft_2t_plus_1
        << ", DPOS total votes count " << dpos_total_votes_count << ", sortition threshold is " << sortition_threshold;
    throw std::logic_error(err.str());
  }

  auto first_cert_vote_round = cert_votes_[0]->getRound();
  for (auto& v : cert_votes_) {
    // Any info is wrong that can determine the synced PBFT block comes from a malicious player
    if (v->getType() != cert_vote_type) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk_->getBlockHash() << ", cert vote " << v->getHash() << " has wrong vote type "
          << v->getType();
      throw std::logic_error(err.str());
    } else if (v->getRound() != first_cert_vote_round) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk_->getBlockHash() << ", cert vote " << v->getHash()
          << " has a different vote round " << v->getRound() << ", compare to first cert vote "
          << cert_votes_[0]->getHash() << " has vote round " << first_cert_vote_round;
      throw std::logic_error(err.str());
    } else if (v->getStep() != 3) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk_->getBlockHash() << ", cert vote " << v->getHash() << " has wrong vote step "
          << v->getStep();
      throw std::logic_error(err.str());
    } else if (v->getBlockHash() != pbft_blk_->getBlockHash()) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk_->getBlockHash() << ", cert vote " << v->getHash()
          << " has wrong vote block hash " << v->getBlockHash();
      throw std::logic_error(err.str());
    }

    if (auto result = v->validate(dpos_total_votes_count, sortition_threshold); !result.first) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk_->getBlockHash() << ", cert vote " << v->getHash()
          << " failed validation: " << result.second;
      throw std::logic_error(err.str());
    }

    assert(v->getWeight());
    total_votes += v->getWeight().value();
  }

  if (total_votes < pbft_2t_plus_1) {
    std::stringstream err;
    err << "PBFT block " << pbft_blk->getBlockHash() << " has a wrong number of cert votes. There are "
        << cert_votes.size() << " cert votes with the block. But 2t+1 is " << pbft_2t_plus_1
        << ", DPOS total votes count " << dpos_total_votes_count << ", sortition threshold is " << sortition_threshold;
    throw std::logic_error(err.str());
  }
}

}  // namespace taraxa
