#include "filter_api.hpp"

#include "../util.hpp"

namespace taraxa::aleth {
using namespace std;
using namespace dev;
using namespace eth;
using namespace util;

struct FilterAPIImpl : virtual FilterAPI {
  optional<LogFilter> getLogFilter(FilterID id) const override { return std::nullopt; }

  FilterID newBlockFilter() override { return 0; }

  FilterID newPendingTransactionFilter() override { return 0; }

  FilterID newLogFilter(LogFilter const& _filter) override { return 0; }

  bool uninstallFilter(FilterID id) override { return false; }

  void poll(FilterID id, Consumer const& consumer) override {}

  void note_block(h256 const& blk_hash) override {}

  void note_pending_transactions(RangeView<h256> const& trx_hashes) override {}

  void note_receipts(RangeView<TransactionReceipt> const& receipts) override {}
};

unique_ptr<FilterAPI> NewFilterAPI() { return u_ptr(new FilterAPIImpl); }

}  // namespace taraxa::aleth
