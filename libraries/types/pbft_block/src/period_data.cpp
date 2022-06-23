#include "pbft/period_data.hpp"

#include <libdevcore/CommonJS.h>

#include "dag/dag_block.hpp"
#include "pbft/pbft_block.hpp"
#include "transaction/transaction.hpp"
#include "vote/vote.hpp"

namespace taraxa {

using namespace std;

PeriodData::PeriodData(std::shared_ptr<PbftBlock> pbft_blk,
                       std::vector<std::shared_ptr<Vote>> const& previous_block_cert_votes)
    : pbft_blk(std::move(pbft_blk)), previous_block_cert_votes(previous_block_cert_votes) {}

PeriodData::PeriodData(dev::RLP&& rlp) {
  auto it = rlp.begin();
  pbft_blk = std::make_shared<PbftBlock>(*it++);
  for (auto const vote_rlp : *it++) {
    previous_block_cert_votes.emplace_back(std::make_shared<Vote>(vote_rlp));
  }

  for (auto const dag_block_rlp : *it++) {
    dag_blocks.emplace_back(dag_block_rlp);
  }

  for (auto const trx_rlp : *it++) {
    transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
  }
}

PeriodData::PeriodData(bytes const& all_rlp) : PeriodData(dev::RLP(all_rlp)) {}

bytes PeriodData::rlp() const {
  dev::RLPStream s(kItemCount);
  s.appendRaw(pbft_blk->rlp(true));
  s.appendList(previous_block_cert_votes.size());
  for (auto const& v : previous_block_cert_votes) {
    s.appendRaw(v->rlp(true));
  }
  s.appendList(dag_blocks.size());
  for (auto const& b : dag_blocks) {
    s.appendRaw(b.rlp(true));
  }
  s.appendList(transactions.size());
  for (auto const& t : transactions) {
    s.appendRaw(t->rlp());
  }
  return s.invalidate();
}

void PeriodData::clear() {
  pbft_blk.reset();
  dag_blocks.clear();
  transactions.clear();
  previous_block_cert_votes.clear();
}

void PeriodData::hasEnoughValidCertVotes(const std::vector<std::shared_ptr<Vote>>& votes, size_t dpos_total_votes_count,
                                         size_t sortition_threshold, size_t pbft_2t_plus_1,
                                         std::function<size_t(addr_t const&)> const& dpos_eligible_vote_count) const {
  if (votes.empty()) {
    throw std::logic_error("No cert votes provided! The synced PBFT block comes from a malicious player.");
  }
  size_t total_votes = 0;
  auto first_cert_vote_round = votes[0]->getRound();
  for (auto& v : votes) {
    // Any info is wrong that can determine the synced PBFT block comes from a malicious player
    if (v->getType() != cert_vote_type) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash() << " has wrong vote type "
          << v->getType();
      throw std::logic_error(err.str());
    } else if (v->getRound() != first_cert_vote_round) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
          << " has a different vote round " << v->getRound() << ", compare to first cert vote " << votes[0]->getHash()
          << " has vote round " << first_cert_vote_round;
      throw std::logic_error(err.str());
    } else if (v->getStep() != 3) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash() << " has wrong vote step "
          << v->getStep();
      throw std::logic_error(err.str());
    } else if (v->getBlockHash() != pbft_blk->getBlockHash()) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
          << " has wrong vote block hash " << v->getBlockHash();
      throw std::logic_error(err.str());
    }
    auto stake = dpos_eligible_vote_count(v->getVoterAddr());
    try {
      v->validate(stake, dpos_total_votes_count, sortition_threshold);
    } catch (const std::logic_error& e) {
      std::stringstream err;
      err << "For PBFT block " << pbft_blk->getBlockHash() << ", cert vote " << v->getHash()
          << " failed validation: " << e.what();
      throw std::logic_error(err.str());
    }

    assert(v->getWeight());
    total_votes += v->getWeight().value();
  }

  if (total_votes < pbft_2t_plus_1) {
    std::stringstream err;
    err << "PBFT block " << pbft_blk->getBlockHash() << " has a wrong number of cert votes. There are " << votes.size()
        << " cert votes with the block. But 2t+1 is " << pbft_2t_plus_1 << ", DPOS total votes count "
        << dpos_total_votes_count << ", sortition threshold is " << sortition_threshold;
    throw std::logic_error(err.str());
  }
}

std::ostream& operator<<(std::ostream& strm, PeriodData const& b) {
  strm << "[PeriodData] : " << b.pbft_blk << " , num of votes " << b.previous_block_cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa