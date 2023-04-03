#pragma once
#include <list>

#include "common/types.hpp"

namespace taraxa {
template <class T>
struct cache {
  std::unordered_map<PbftPeriod, std::shared_ptr<T>> cached_data;
  mutable std::list<PbftPeriod> access_order;
  uint64_t capacity = 25;

  std::shared_ptr<T> get(PbftPeriod p) const {
    auto itr = cached_data.find(p);
    if (itr != cached_data.end()) {
      access_order.remove(p);
      access_order.push_back(p);
      return itr->second;
    }
    return nullptr;
  }

  void save(PbftPeriod p, std::shared_ptr<T> data) {
    if (cached_data.contains(p)) {
      return;
    }
    cached_data.emplace(p, std::move(data));
    access_order.push_back(p);
    if (access_order.size() > capacity) {
      auto to_remove = access_order.front();
      access_order.pop_front();
      cached_data.erase(to_remove);
    }
  }
};
}  // namespace taraxa