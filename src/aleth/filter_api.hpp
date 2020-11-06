#ifndef TARAXA_NODE_TARAXA_ALETH_FILTER_API_HPP_
#define TARAXA_NODE_TARAXA_ALETH_FILTER_API_HPP_

#include <libweb3jsonrpc/Eth.h>

#include "../util/range_view.hpp"

namespace taraxa::aleth {

struct FilterAPI : virtual dev::rpc::Eth::FilterAPI {
  virtual ~FilterAPI() {}
  virtual void note_block(dev::h256 const& blk_hash) = 0;
  virtual void note_pending_transactions(util::RangeView<dev::h256> const& trx_hashes) = 0;
  virtual void note_receipts(util::RangeView<dev::eth::TransactionReceipt> const& receipts) = 0;
};

std::unique_ptr<FilterAPI> NewFilterAPI();

}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_TARAXA_ALETH_FILTER_API_HPP_
