#ifndef TARAXA_NODE_TARAXA_ALETH_FILTER_API_HPP_
#define TARAXA_NODE_TARAXA_ALETH_FILTER_API_HPP_

#include <libweb3jsonrpc/Eth.h>

#include "../util/range_view.hpp"

namespace taraxa::aleth::filter_api {
using namespace std;
using namespace dev;
using namespace eth;
using namespace util;

struct FilterAPI : dev::rpc::Eth::FilterAPI {
  optional<LogFilter> getLogFilter(FilterID id) const override;
  FilterID newBlockFilter() override;
  FilterID newPendingTransactionFilter() override;
  FilterID newLogFilter(LogFilter const& _filter) override;
  bool uninstallFilter(FilterID id) override;
  void poll(FilterID id, Consumer const& consumer) override;

  void note_block(h256 const& blk_hash);
  void note_pending_transactions(RangeView<h256> const& trx_hashes);
  void note_receipts(RangeView<TransactionReceipt> const& receipts);
};

}  // namespace taraxa::aleth::filter_api

namespace taraxa::aleth {
using filter_api::FilterAPI;
}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_TARAXA_ALETH_FILTER_API_HPP_
