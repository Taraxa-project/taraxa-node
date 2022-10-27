#include "pbft/soft_voted_block_data.hpp"

#include "pbft/pbft_block.hpp"
#include "vote/vote.hpp"

namespace taraxa {

TwoTPlusOneSoftVotedBlockData::TwoTPlusOneSoftVotedBlockData(dev::RLP const& rlp) {
  assert(rlp.itemCount() == TwoTPlusOneSoftVotedBlockData::kStandardDataItemCount ||
         rlp.itemCount() == TwoTPlusOneSoftVotedBlockData::kExtendedDataItemCount);

  auto it = rlp.begin();

  // Parse block if present
  block_data_ = std::nullopt;
  // block is optional as node might not had the actual block when it observed 2t+1 soft votes and saved it
  if (rlp.itemCount() == TwoTPlusOneSoftVotedBlockData::kExtendedDataItemCount) {
    block_data_ = {std::make_shared<PbftBlock>(*it++), false};
  }

  // Parse round
  round_ = (*it++).toInt<uint64_t>();

  // Parse block hash
  block_hash_ = (*it++).toHash<blk_hash_t>();
  if (block_data_.has_value()) {
    assert(block_data_->first->getBlockHash() == block_hash_);
  }

  // Parse votes
  soft_votes_.reserve((*it).itemCount());
  for (auto const vote_rlp : *it) {
    auto vote = std::make_shared<Vote>(vote_rlp);
    assert(vote->getType() == PbftVoteTypes::soft_vote);
    assert(vote->getRound() == round_);
    assert(vote->getBlockHash() == block_hash_);
    if (block_data_.has_value()) {
      assert(vote->getPeriod() == block_data_->first->getPeriod());
    }

    soft_votes_.push_back(std::move(vote));
  }

  assert(!soft_votes_.empty());
}

bytes TwoTPlusOneSoftVotedBlockData::rlp() const {
  dev::RLPStream s;
  // block is optional as node might not had the actual block when it observed 2t+1 soft votes and saved it
  if (block_data_.has_value()) {
    assert(block_data_->first != nullptr);
    assert(block_data_->first->getBlockHash() == block_hash_);
    s.appendList(TwoTPlusOneSoftVotedBlockData::kExtendedDataItemCount);  // block + round + block_hash + votes
    s.appendRaw(block_data_->first->rlp(true));
  } else {
    s.appendList(TwoTPlusOneSoftVotedBlockData::kStandardDataItemCount);  // round + block_hash + votes
  }

  s.append(round_);
  s.append(block_hash_);
  s.appendList(soft_votes_.size());
  for (auto const& vote : soft_votes_) {
    assert(vote->getType() == PbftVoteTypes::soft_vote);
    assert(vote->getRound() == round_);
    assert(vote->getBlockHash() == block_hash_);
    if (block_data_.has_value()) {
      assert(vote->getPeriod() == block_data_->first->getPeriod());
    }

    s.appendRaw(vote->rlp(true, true));
  }

  return s.invalidate();
}

}  // namespace taraxa
