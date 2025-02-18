#pragma once

#include <json/json.h>

#include <atomic>
#include <thread>

#include "thread_pool.hpp"

namespace taraxa::util {

template <class S, class FN>
Json::Value transformToJsonParallel(const S& source, FN op) {
  if (source.empty()) {
    return Json::Value(Json::arrayValue);
  }
  static util::ThreadPool executor{std::thread::hardware_concurrency() / 2};

  Json::Value out(Json::arrayValue);
  out.resize(source.size());
  std::atomic_uint processed = 0;
  for (unsigned i = 0; i < source.size(); ++i) {
    executor.post([&, i]() {
      out[i] = op(source[i], i);
      ++processed;
    });
  }

  while (true) {
    if (processed == source.size()) {
      break;
    }
  }
  return out;
}

Json::Value mergeJsons(Json::Value&& o1, Json::Value&& o2);

}  // namespace taraxa::util
