#pragma once

#include <chrono>

#include "LogReader.hpp"
#include "data.hpp"

namespace taraxa::net::rpc::eth {

// TODO taraxa simple exception
DEV_SIMPLE_EXCEPTION(WatchLimitExceeded);

using dummy_t = uint8_t;
template <typename InputType, typename OutputType = InputType, typename Params = dummy_t>
struct WatchGroup {
  using time_point = chrono::system_clock::time_point;
  using WatchID = uint64_t;

  struct Watch {
    Params params;
    time_point last_touched;
    vector<OutputType> updates;
  };

 private:
  WatchGroupConfig cfg;
  function<void(Params const&, InputType const&, function<void(OutputType const&)> const&)> updater;
  unordered_map<WatchID, Watch> watches;
  shared_mutex watches_mu;
  WatchID watch_id_seq = 0;

 public:
  WatchGroup(WatchGroupConfig const& cfg = {}, decltype(updater)&& updater = {}) : cfg(cfg), updater(move(updater)) {
    assert(cfg.idle_timeout.count() != 0);
    if constexpr (is_same_v<InputType, OutputType>) {
      if (!updater) {
        updater = [](auto const& params, auto const& input, auto const& do_update) { do_update(input); };
      }
    }
  }

  WatchID install_watch(Params&& predicate_params = {}) {
    unique_lock l(watches_mu);
    if (watches.size() == cfg.max_watches) {
      throw WatchLimitExceeded();
    }
    auto id = ++watch_id_seq;
    watches[id] = {move(predicate_params), chrono::system_clock::now()};
    return id;
  }

  bool uninstall_watch(WatchID watch_id) {
    unique_lock l(watches_mu);
    return watches.erase(watch_id);
  }

  void uninstall_stale_watches() {
    unique_lock l(watches_mu);
    auto t_now = chrono::system_clock::now();
    bool did_uninstall = false;
    for (auto& [id, watch] : watches) {
      if (cfg.idle_timeout <= (t_now - watch.last_touched).seconds()) {
        watches.erase(id);
        did_uninstall = true;
      }
    }
    if (auto num_buckets = watches.bucket_count(); did_uninstall && (1 << 10) < num_buckets) {
      if (auto desired_num_buckets = 1 << uint(ceil(log2(watches.size()))); desired_num_buckets < num_buckets) {
        watches.rehash(desired_num_buckets);
      }
    }
  }

  optional<Params> get_watch_params(WatchID watch_id) {
    shared_lock l(watches_mu);
    if (auto entry = watches.find(watch_id); entry != watches.end()) {
      return entry->second.params;
    }
    return {};
  }

  void process_updates(util::RangeView<InputType> const& input) {
    input.for_each([this](auto const& obj_in) {
      shared_lock l(watches_mu);
      for (auto& entry : watches) {
        auto& watch = entry.second;
        updater(watch.params, obj_in, [&](auto const& obj_out) { watch.updates.push_back(obj_out); });
      }
    });
  }

  vector<InputType> poll(WatchID watch_id) {
    vector<InputType> ret;
    shared_lock l(watches_mu);
    if (auto entry = watches.find(watch_id); entry != watches.end()) {
      auto& watch = entry->second;
      swap(ret, watch.updates);
      watch.last_touched = chrono::system_clock::now();
    }
    return ret;
  }
};

class Watches {
  using WatchID = string;

  WatchGroup<h256> new_blocks;
  WatchGroup<h256> new_transactions;
  WatchGroup<TransactionReceipt, LogFilter> logs;
  thread watch_cleaner;
  atomic<bool> destructor_called = false;

 public:
  Watches(WatchesConfig const& cfg)
      : new_blocks(cfg.new_blocks),
        new_transactions(cfg.new_transactions),
        logs(cfg.logs,
             [](auto const& log_filter, auto const& receipt, auto const& do_update) {
               //
               //
             }),
        watch_cleaner([this] {
          while (!destructor_called) {
            //
          }
        }) {}

  ~Watches() {
    destructor_called = true;
    watch_cleaner.join();
  }

  optional<LogFilter> getLogFilter(WatchID const& id) const { return {}; }
  WatchID newBlockFilter() { return 0; }
  WatchID newPendingTransactionFilter() { return 0; }
  WatchID newLogFilter(LogFilter&& _filter) { return 0; }
  bool uninstallFilter(WatchID const& id) { return false; }

  void note_block(h256 const& blk_hash) { new_blocks.process_updates(vector{blk_hash}); }
  void note_pending_transactions(RangeView<h256> const& trx_hashes) { new_transactions.process_updates(trx_hashes); }
  void note_receipts(RangeView<TransactionReceipt> const& receipts) { logs.process_updates(receipts); }

  Json::Value poll(WatchID const& id) { return Json::Value(Json::arrayValue); }
};

}  // namespace taraxa::net::rpc::eth