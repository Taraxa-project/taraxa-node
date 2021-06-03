#pragma once

#include <chrono>
#include <queue>

#include "LogFilter.hpp"
#include "data.hpp"

namespace taraxa::net::rpc::eth {
using namespace chrono;

// TODO taraxa simple exception
DEV_SIMPLE_EXCEPTION(WatchLimitExceeded);

using WatchID = uint64_t;

inline auto const watch_id_type_mask_bits = WatchType(ceil(log2(float(COUNT))));

struct placeholder_t {};
template <WatchType type_, typename InputType_, typename OutputType_ = placeholder_t, typename Params = placeholder_t>
struct WatchGroup {
  static constexpr auto type = type_;
  using InputType = InputType_;
  using OutputType = conditional_t<is_same_v<OutputType_, placeholder_t>, InputType, OutputType_>;
  using time_point = high_resolution_clock::time_point;
  using Updater = function<void(Params const&,     //
                                InputType const&,  //
                                function<void(OutputType const&)> const& /*do_update*/)>;

  struct Watch {
    Params params;
    time_point last_touched;
    vector<OutputType> updates;
  };

 private:
  WatchGroupConfig cfg_;
  Updater updater_;
  mutable unordered_map<WatchID, Watch> watches_;
  mutable shared_mutex watches_mu_;
  mutable WatchID watch_id_seq_ = 0;

 public:
  WatchGroup(WatchesConfig const& cfg = {}, Updater&& updater = {}) : cfg_(cfg[type]), updater_(move(updater)) {
    assert(cfg_.idle_timeout.count() != 0);
    if constexpr (is_same_v<InputType, OutputType>) {
      if (!updater) {
        updater = [](auto const&, auto const& input, auto const& do_update) { do_update(input); };
      }
    }
  }

  WatchID install_watch(Params&& params = {}) const {
    unique_lock l(watches_mu_);
    if (cfg_.max_watches && watches_.size() == cfg_.max_watches) {
      throw WatchLimitExceeded();
    }
    auto id = ((++watch_id_seq_) << watch_id_type_mask_bits) + type;
    watches_.insert_or_assign(id, Watch{move(params), high_resolution_clock::now(), {}});
    return id;
  }

  bool uninstall_watch(WatchID watch_id) const {
    unique_lock l(watches_mu_);
    return watches_.erase(watch_id);
  }

  void uninstall_stale_watches() const {
    unique_lock l(watches_mu_);
    bool did_uninstall = false;
    for (auto& [id, watch] : watches_) {
      if (cfg_.idle_timeout <= duration_cast<seconds>(high_resolution_clock::now() - watch.last_touched)) {
        watches_.erase(id);
        did_uninstall = true;
      }
    }
    if (auto num_buckets = watches_.bucket_count(); did_uninstall && (1 << 10) < num_buckets) {
      if (size_t desired_num_buckets = 1 << uint(ceil(log2(watches_.size()))); desired_num_buckets != num_buckets) {
        watches_.rehash(desired_num_buckets);
      }
    }
  }

  optional<Params> get_watch_params(WatchID watch_id) const {
    shared_lock l(watches_mu_);
    if (auto entry = watches_.find(watch_id); entry != watches_.end()) {
      return entry->second.params;
    }
    return {};
  }

  void process_update(InputType const& obj_in) const {
    shared_lock l(watches_mu_);
    for (auto& entry : watches_) {
      auto& watch = entry.second;
      updater_(watch.params, obj_in, [&](auto const& obj_out) { watch.updates.push_back(obj_out); });
    }
  }

  auto poll(WatchID watch_id) const {
    vector<OutputType> ret;
    shared_lock l(watches_mu_);
    if (auto entry = watches_.find(watch_id); entry != watches_.end()) {
      auto& watch = entry->second;
      swap(ret, watch.updates);
      watch.last_touched = high_resolution_clock::now();
    }
    return ret;
  }
};

struct Watches {
  WatchesConfig const cfg;

  WatchGroup<WatchType::new_blocks, h256> const new_blocks{cfg};
  WatchGroup<WatchType::new_transactions, h256> const new_transactions{cfg};
  WatchGroup<WatchType::logs,  //
             pair<ExtendedTransactionLocation const&, TransactionReceipt const&>, LocalisedLogEntry, LogFilter> const
      logs{
          cfg,
          [](auto const& log_filter, auto const& input, auto const& do_update) {
            auto const& [trx_loc, receipt] = input;
            log_filter.match_one(trx_loc, receipt, do_update);
          },
      };

  template <typename Visitor>
  auto visit(WatchType type, Visitor&& visitor) {
    switch (type) {
      case WatchType::new_blocks:
        return visitor(&new_blocks);
      case WatchType::new_transactions:
        return visitor(&new_transactions);
      case WatchType::logs:
        return visitor(&logs);
      default:
        assert(false);
    }
  }

 private:
  thread watch_cleaner_;
  atomic<bool> destructor_called_ = false;

 public:
  Watches(WatchesConfig const& _cfg)
      : cfg(_cfg),  //
        watch_cleaner_([this] {
          struct WatchEntry {
            WatchType type;
            high_resolution_clock::time_point deadline;

            bool operator<(WatchEntry const& other) const { return deadline < other.deadline; }
          };
          priority_queue<WatchEntry> q;
          for (uint type = 0; type < COUNT; ++type) {
            q.push({WatchType(type), high_resolution_clock::now() + cfg[type].idle_timeout});
          }
          for (;;) {
            auto soonest = q.top();
            q.pop();
            for (;;) {
              if (destructor_called_) {
                return;
              }
              auto t_0 = high_resolution_clock::now();
              if (soonest.deadline <= t_0) {
                break;
              }
              static nanoseconds const sleep_limit = 2s;
              this_thread::sleep_for(min(sleep_limit, duration_cast<nanoseconds>(soonest.deadline - t_0)));
            }
            visit(soonest.type, [](auto watch) { watch->uninstall_stale_watches(); });
            soonest.deadline = high_resolution_clock::now() + cfg[soonest.type].idle_timeout;
            q.push(soonest);
          }
        }) {}

  ~Watches() {
    destructor_called_ = true;
    watch_cleaner_.join();
  }

  template <typename Visitor>
  auto visit_by_id(WatchID watch_id, Visitor&& visitor) {
    if (auto type = WatchType(watch_id & ((1 << watch_id_type_mask_bits) - 1)); type < COUNT) {
      return visit(type, forward<Visitor>(visitor));
    }
    return visitor(decltype (&new_blocks)(nullptr));
  }
};

}  // namespace taraxa::net::rpc::eth