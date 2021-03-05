#pragma once

#include "data.hpp"

namespace taraxa::net::rpc::eth {

struct Filters {
  using FilterID = uint64_t;

  optional<LogFilter> getLogFilter(FilterID id) const { return {}; }
  FilterID newBlockFilter() { return 0; }
  FilterID newPendingTransactionFilter() { return 0; }
  FilterID newLogFilter(LogFilter&& _filter) { return 0; }
  bool uninstallFilter(FilterID id) { return false; }

  void note_block(h256 const& blk_hash) {}
  void note_pending_transactions(RangeView<h256> const& trx_hashes) {}
  void note_receipts(RangeView<TransactionReceipt> const& receipts) {}

  Json::Value poll(FilterID id) { return Json::Value(Json::arrayValue); }
};

}  // namespace taraxa::net::rpc::eth