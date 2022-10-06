#include "pbft/soft_voted_block_data.hpp"

#include "pbft/pbft_block.hpp"
#include "vote/vote.hpp"

namespace taraxa {

TwoTPlusOneSoftVotedBlockData::TwoTPlusOneSoftVotedBlockData(dev::RLP const& rlp) {
  assert(rlp.itemCount() == TwoTPlusOneSoftVotedBlockData::kStandardDataItemCount ||
         rlp.itemCount() == TwoTPlusOneSoftVotedBlockData::kExtendedDataItemCount);

  auto it = rlp.begin();

  // Parse block if present
  block_ = nullptr;
  // block is optional as node might not had the actual block when it observed 2t+1 soft votes and saved it
  if (rlp.itemCount() == TwoTPlusOneSoftVotedBlockData::kExtendedDataItemCount) {
    block_ = std::make_shared<PbftBlock>(*it++);
  }

  // Parse round
  round_ = (*it++).toInt<uint64_t>();

  // Parse block hash
  block_hash_ = (*it++).toHash<blk_hash_t>();
  if (block_) {
    assert(block_->getBlockHash() == block_hash_);
  }

  // Parse votes
  soft_votes_.reserve((*it).itemCount());
  for (auto const vote_rlp : *it) {
    auto vote = std::make_shared<Vote>(vote_rlp);
    assert(vote->getType() == PbftVoteTypes::soft_vote_type);
    assert(vote->getRound() == round_);
    assert(vote->getBlockHash() == block_hash_);
    if (block_) {
      assert(vote->getPeriod() == block_->getPeriod());
    }

    soft_votes_.push_back(std::move(vote));
  }

  assert(!soft_votes_.empty());
}

bytes TwoTPlusOneSoftVotedBlockData::rlp() const {
  dev::RLPStream s;
  // block is optional as node might not had the actual block when it observed 2t+1 soft votes and saved it
  if (block_) {
    assert(block_->getBlockHash() == block_hash_);
    s.appendList(TwoTPlusOneSoftVotedBlockData::kExtendedDataItemCount);  // block + round + block_hash + votes
    s.appendRaw(block_->rlp(true));
  } else {
    s.appendList(TwoTPlusOneSoftVotedBlockData::kStandardDataItemCount);  // round + block_hash + votes
  }

  s.append(round_);
  s.append(block_hash_);
  s.appendList(soft_votes_.size());
  for (auto const& vote : soft_votes_) {
    assert(vote->getType() == PbftVoteTypes::soft_vote_type);
    assert(vote->getRound() == round_);
    assert(vote->getBlockHash() == block_hash_);
    if (block_) {
      assert(vote->getPeriod() == block_->getPeriod());
    }

    s.appendRaw(vote->rlp(true, true));
  }

  return s.invalidate();
}

}  // namespace taraxa
