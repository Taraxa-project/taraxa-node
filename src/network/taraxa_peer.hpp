#pragma once

#include <boost/noncopyable.hpp>
#include <atomic>

#include <libp2p/Common.h>
#include "util/util.hpp"

namespace taraxa {

class TaraxaPeer : public boost::noncopyable {
 public:
  TaraxaPeer()
      : known_blocks_(10000, 1000),
        known_transactions_(100000, 10000),
        known_pbft_blocks_(10000, 1000),
        known_votes_(10000, 1000) {}
  explicit TaraxaPeer(dev::p2p::NodeID peer_id)
      : peer_id_(peer_id),
        known_blocks_(10000, 1000),
        known_transactions_(100000, 10000),
        known_pbft_blocks_(10000, 1000),
        known_votes_(10000, 1000) {}

  bool isBlockKnown(blk_hash_t const &_hash) const { return known_blocks_.count(_hash); }
  void markBlockAsKnown(blk_hash_t const &_hash) { known_blocks_.insert(_hash); }

  bool isTransactionKnown(trx_hash_t const &_hash) const { return known_transactions_.count(_hash); }
  void markTransactionAsKnown(trx_hash_t const &_hash) { known_transactions_.insert(_hash); }

  void clearAllKnownBlocksAndTransactions() {
    known_transactions_.clear();
    known_blocks_.clear();
  }

  // PBFT
  bool isVoteKnown(vote_hash_t const &_hash) const { return known_votes_.count(_hash); }
  void markVoteAsKnown(vote_hash_t const &_hash) { known_votes_.insert(_hash); }

  bool isPbftBlockKnown(blk_hash_t const &_hash) const { return known_pbft_blocks_.count(_hash); }
  void markPbftBlockAsKnown(blk_hash_t const &_hash) { known_pbft_blocks_.insert(_hash); }

  bool isAlive(uint16_t max_check_count) {
    alive_check_count_++;
    return alive_check_count_ <= max_check_count;
  }

  void setAlive() { alive_check_count_ = 0; }

  bool passed_initial_ = false;
  bool syncing_ = false;
  uint64_t dag_level_ = 0;
  uint64_t pbft_chain_size_ = 0;
  uint64_t pbft_round_ = 1;
  size_t pbft_previous_round_next_votes_size_ = 0;

 private:
  dev::p2p::NodeID peer_id_;

  ExpirationCache<blk_hash_t> known_blocks_;
  ExpirationCache<trx_hash_t> known_transactions_;
  // PBFT
  ExpirationCache<blk_hash_t> known_pbft_blocks_;
  ExpirationCache<vote_hash_t> known_votes_;  // for peers

  std::atomic<uint16_t> alive_check_count_ = 0;
};

}  // namespace taraxa