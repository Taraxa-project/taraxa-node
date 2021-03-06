#pragma once

#include <functional>

namespace taraxa::util {
using task_t = std::function<void()>;
using task_executor_t = std::function<void(task_t &&)>;

inline task_executor_t current_thread_executor() {
  return [](auto &&task) { task(); };
}

}  // namespace taraxa::util