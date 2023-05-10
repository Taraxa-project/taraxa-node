#include "pbft/period_data.hpp"

#include <libdevcore/CommonJS.h>

#include "dag/dag_block.hpp"
#include "pbft/pbft_block.hpp"
#include "transaction/transaction.hpp"
#include "vote/vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa {

using namespace std;

PeriodData::PeriodData(std::shared_ptr<PbftBlock> pbft_blk,
                       const std::vector<std::shared_ptr<Vote>>& previous_block_cert_votes)
    : pbft_blk(std::move(pbft_blk)), previous_block_cert_votes(previous_block_cert_votes) {}

PeriodData::PeriodData(const dev::RLP& rlp) {
  auto it = rlp.begin();
  pbft_blk = std::make_shared<PbftBlock>(*it++);

  const auto votes_bundle_rlp = *it++;
  if (pbft_blk->getPeriod() > 1) [[likely]] {
    previous_block_cert_votes = decodeVotesBundleRlp(votes_bundle_rlp);
  }

  for (auto const dag_block_rlp : *it++) {
    dag_blocks.emplace_back(dag_block_rlp);
  }

  for (auto const trx_rlp : *it) {
    transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
  }
}

PeriodData::PeriodData(bytes const& all_rlp) : PeriodData(dev::RLP(all_rlp)) {}

bytes PeriodData::rlp() const {
  dev::RLPStream s(kRlpItemCount);
  s.appendRaw(pbft_blk->rlp(true));

  if (pbft_blk->getPeriod() > 1) [[likely]] {
    s.appendRaw(encodeVotesBundleRlp(previous_block_cert_votes, false));
  } else {
    s.append("");
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

std::ostream& operator<<(std::ostream& strm, PeriodData const& b) {
  strm << "[PeriodData] : " << b.pbft_blk << " , num of votes " << b.previous_block_cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa