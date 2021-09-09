#include "pbft/sync_block.hpp"

#include <libdevcore/CommonJS.h>

#include "dag/block.hpp"
#include "pbft/block.hpp"
#include "transaction/transaction.hpp"
#include "vote/vote.hpp"

namespace taraxa {

using namespace std;

SyncBlock::SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<std::shared_ptr<Vote>> const& cert_votes)
    : pbft_blk(std::move(pbft_blk)), cert_votes(cert_votes) {}

SyncBlock::SyncBlock(dev::RLP&& rlp) {
  auto it = rlp.begin();
  pbft_blk = std::make_shared<PbftBlock>(*it++);
  for (auto const vote_rlp : *it++) {
    cert_votes.emplace_back(std::make_shared<Vote>(vote_rlp));
  }

  for (auto const dag_block_rlp : *it++) {
    dag_blocks.emplace_back(dag_block_rlp);
  }

  for (auto const trx_rlp : *it) {
    transactions.emplace_back(trx_rlp);
  }
}

SyncBlock::SyncBlock(bytes const& all_rlp) : SyncBlock(dev::RLP(all_rlp)) {}

bytes SyncBlock::rlp() const {
  dev::RLPStream s(4);
  s.appendRaw(pbft_blk->rlp(true));
  s.appendList(cert_votes.size());
  for (auto const& v : cert_votes) {
    s.appendRaw(v->rlp(true));
  }
  s.appendList(dag_blocks.size());
  for (auto const& b : dag_blocks) {
    s.appendRaw(b.rlp(true));
  }
  s.appendList(transactions.size());
  for (auto const& t : transactions) {
    s.appendRaw(*t.rlp());
  }
  return s.out();
}

void SyncBlock::clear() {
  pbft_blk.reset();
  dag_blocks.clear();
  transactions.clear();
  cert_votes.clear();
}

bool SyncBlock::hasEnoughValidCertVotes(size_t dpos_total_votes_count, size_t sortition_threshold,
                                        size_t pbft_2t_plus_1) const {
  if (cert_votes.empty()) {
    LOG(log_er_) << "No any cert votes! The synced PBFT block comes from a "
                    "malicious player.";
    return false;
  }

  std::vector<std::shared_ptr<Vote>> valid_votes;
  auto first_cert_vote_round = cert_votes[0]->getRound();

  for (auto& v : cert_votes) {
    // Any info is wrong that can determine the synced PBFT block comes from a malicious player
    if (v->getType() != cert_vote_type) {
      LOG(log_er_) << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
                   << " has wrong vote type " << v->getType();
      break;
    } else if (v->getRound() != first_cert_vote_round) {
      LOG(log_er_) << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
                   << " has a different vote round " << v->getRound() << ", compare to first cert vote "
                   << cert_votes[0]->getHash() << " has vote round " << first_cert_vote_round;
      break;
    } else if (v->getStep() != 3) {
      LOG(log_er_) << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
                   << " has wrong vote step " << v->getStep();
      break;
    } else if (v->getBlockHash() != pbft_blk->getBlockHash()) {
      LOG(log_er_) << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
                   << " has wrong vote block hash " << v->getBlockHash();
      break;
    }

    if (v->validate(dpos_total_votes_count, sortition_threshold)) {
      valid_votes.emplace_back(v);
    } else {
      LOG(log_wr_) << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
                   << " failed validation";
    }
  }

  if (valid_votes.size() < pbft_2t_plus_1) {
    LOG(log_er_) << "PBFT block " << pbft_blk->getBlockHash() << " with " << cert_votes.size() << " cert votes. Has "
                 << valid_votes.size() << " valid cert votes. 2t+1 is " << pbft_2t_plus_1 << ", DPOS total votes count "
                 << dpos_total_votes_count << ", sortition threshold is " << sortition_threshold;
  }

  return valid_votes.size() >= pbft_2t_plus_1;
}

std::ostream& operator<<(std::ostream& strm, SyncBlock const& b) {
  strm << "[SyncBlock] : " << b.pbft_blk << " , num of votes " << b.cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa