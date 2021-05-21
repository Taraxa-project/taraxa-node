#pragma once

#include <libweb3jsonrpc/Eth.h>

namespace taraxa::aleth {

/*
 * We are not using these API's in taraxa but they still have to be passed to dev::rpc::Eth constructor
 */

struct DummyPendingBlock : dev::rpc::Eth::PendingBlock {
  dev::eth::BlockDetails details() const override { return {}; }
  uint64_t transactionsCount() const override { return 0; }
  uint64_t transactionsCount(dev::Address const& /*from*/) const override { return 0; }
  dev::eth::Transactions transactions() const override { return {}; }
  std::optional<dev::eth::Transaction> transaction(unsigned /*index*/) const override { return std::nullopt; }
  dev::h256s transactionHashes() const override { return {}; }
  dev::eth::BlockHeader header() const override { return {}; }
};

struct DummyFilterAPI : dev::rpc::Eth::FilterAPI {
  std::optional<dev::eth::LogFilter> getLogFilter(FilterID /*id*/) const override { return std::nullopt; }
  FilterID newBlockFilter() override { return 0; }
  FilterID newPendingTransactionFilter() override { return 0; }
  FilterID newLogFilter(dev::eth::LogFilter const& /*_filter*/) override { return 0; }
  bool uninstallFilter(FilterID /*id*/) override { return false; }
  void poll(FilterID /*id*/, Consumer const& /*consumer*/) override {}
};

}  // namespace taraxa::aleth
