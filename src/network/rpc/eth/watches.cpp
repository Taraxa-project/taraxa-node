#include "watches.hpp"

namespace taraxa::net::rpc::eth {

GLOBAL_CONST_DEF(watch_id_type_mask_bits, WatchType(ceil(log2(float(COUNT)))))

Watches::Watches(WatchesConfig const& _cfg)
    : cfg_(_cfg),  //
      watch_cleaner_([this] {
        struct WatchEntry {
          WatchType type;
          high_resolution_clock::time_point deadline;

          bool operator<(WatchEntry const& other) const { return deadline < other.deadline; }
        };
        priority_queue<WatchEntry> q;
        mutex mu_for_wait_cv;
        for (uint type = 0; type < COUNT; ++type) {
          q.push({WatchType(type), high_resolution_clock::now() + cfg_[type].idle_timeout});
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
            unique_lock l(mu_for_wait_cv);
            watch_cleaner_wait_cv_.wait_for(l, soonest.deadline - t_0);
          }
          visit(soonest.type, [](auto watch) { watch->uninstall_stale_watches(); });
          soonest.deadline = high_resolution_clock::now() + cfg_[soonest.type].idle_timeout;
          q.push(soonest);
        }
      }) {}

Watches::~Watches() {
  destructor_called_ = true;
  watch_cleaner_wait_cv_.notify_all();
  watch_cleaner_.join();
}

}  // namespace taraxa::net::rpc::eth