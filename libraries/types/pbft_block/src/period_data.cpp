#include "pbft/period_data.hpp"

#include <libdevcore/CommonJS.h>

#include "dag/dag_block_bundle_rlp.hpp"
#include "pbft/pbft_block.hpp"
#include "transaction/transaction.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa {

using namespace std;

PeriodData::PeriodData(std::shared_ptr<PbftBlock> pbft_blk,
                       const std::vector<std::shared_ptr<PbftVote>>& previous_block_cert_votes,
                       std::optional<std::vector<std::shared_ptr<PillarVote>>>&& pillar_votes)
    : pbft_blk(std::move(pbft_blk)),
      previous_block_cert_votes(previous_block_cert_votes),
      pillar_votes_(std::move(pillar_votes)) {}

PeriodData::PeriodData(const dev::RLP& rlp) {
  auto it = rlp.begin();
  pbft_blk = std::make_shared<PbftBlock>(*it++);

  const auto votes_bundle_rlp = *it++;
  if (pbft_blk->getPeriod() > 1) [[likely]] {
    previous_block_cert_votes = decodePbftVotesBundleRlp(votes_bundle_rlp);
  }

  const auto block_bundle_rlp = *it++;
  dag_blocks = decodeDAGBlocksBundleRlp(block_bundle_rlp);

  for (auto const trx_rlp : *it++) {
    transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
  }

  // Pillar votes are optional data of period data since ficus hardfork
  if (rlp.itemCount() == 5) {
    pillar_votes_ = decodePillarVotesBundleRlp(*it);
  }
}

PeriodData::PeriodData(bytes const& all_rlp) : PeriodData(dev::RLP(all_rlp)) {}

bytes PeriodData::rlp() const {
  const auto kRlpSize = pillar_votes_.has_value() ? kBaseRlpItemCount + 1 : kBaseRlpItemCount;
  dev::RLPStream s(kRlpSize);
  s.appendRaw(pbft_blk->rlp(true));

  if (pbft_blk->getPeriod() > 1) [[likely]] {
    s.appendRaw(encodePbftVotesBundleRlp(previous_block_cert_votes));
  } else {
    s.append("");
  }

  s.appendRaw(encodeDAGBlocksBundleRlp(dag_blocks));

  s.appendList(transactions.size());
  for (auto const& t : transactions) {
    s.appendRaw(t->rlp());
  }

  // Pillar votes are optional data of period data since ficus hardfork
  if (pillar_votes_.has_value()) {
    s.appendRaw(encodePillarVotesBundleRlp(*pillar_votes_));
  }

  return s.invalidate();
}

void PeriodData::clear() {
  pbft_blk.reset();
  dag_blocks.clear();
  transactions.clear();
  previous_block_cert_votes.clear();
  pillar_votes_.reset();
}

PeriodData PeriodData::FromOldPeriodData(const dev::RLP& rlp) {
  PeriodData period_data;
  auto it = rlp.begin();
  period_data.pbft_blk = std::make_shared<PbftBlock>(*it++);

  const auto votes_bundle_rlp = *it++;
  if (period_data.pbft_blk->getPeriod() > 1) [[likely]] {
    period_data.previous_block_cert_votes = decodePbftVotesBundleRlp(votes_bundle_rlp);
  }

  for (auto const dag_block_rlp : *it++) {
    period_data.dag_blocks.emplace_back(dag_block_rlp);
  }

  for (auto const trx_rlp : *it++) {
    period_data.transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
  }

  // Pillar votes are optional data of period data since ficus hardfork
  if (rlp.itemCount() == 5) {
    period_data.pillar_votes_ = decodePillarVotesBundleRlp(*it);
  }
  return period_data;
}

std::ostream& operator<<(std::ostream& strm, PeriodData const& b) {
  strm << "[PeriodData] : " << b.pbft_blk << " , num of votes " << b.previous_block_cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa