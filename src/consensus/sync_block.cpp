#include "sync_block.hpp"

#include <libdevcore/CommonJS.h>

#include "consensus/pbft_chain.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa {

using namespace std;

SyncBlock::SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<std::shared_ptr<Vote>> cert_votes)
    : pbft_blk(std::move(pbft_blk)), cert_votes(std::move(cert_votes)) {}

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

std::ostream& operator<<(std::ostream& strm, SyncBlock const& b) {
  strm << "[SyncBlock] : " << b.pbft_blk << " , num of votes " << b.cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa