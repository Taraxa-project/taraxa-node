#include "filter_api.hpp"

namespace taraxa::aleth::filter_api {

optional<LogFilter> FilterAPI::getLogFilter(FilterAPI::FilterID id) const {
  return std::nullopt;
}

FilterAPI::FilterID FilterAPI::newBlockFilter() { return 0; }

FilterAPI::FilterID FilterAPI::newPendingTransactionFilter() { return 0; }

FilterAPI::FilterID FilterAPI::newLogFilter(LogFilter const& _filter) {
  return 0;
}

bool FilterAPI::uninstallFilter(FilterAPI::FilterID id) { return false; }

void FilterAPI::poll(FilterAPI::FilterID id,
                     FilterAPI::Consumer const& consumer) {}

void FilterAPI::note_block(h256 const& blk_hash) {
  //
}
void FilterAPI::note_pending_transactions(RangeView<h256> const& trx_hashes) {
  //
}
void FilterAPI::note_receipts(RangeView<TransactionReceipt> const& receipts) {
  //
}

}  // namespace taraxa::aleth::filter_api
