#pragma once

#include <memory>

namespace taraxa::util {

template <typename T>
auto s_ptr(T *ptr) {
  return std::shared_ptr<T>(ptr);
}

template <typename T>
auto u_ptr(T *ptr) {
  return std::unique_ptr<T>(ptr);
}

template <typename T>
auto s_alloc(T &&value) {
  return std::make_shared<T>(std::move(value));
}

template <typename T>
auto s_alloc(T const &value) {
  return std::make_shared<T>(value);
}

}  // namespace taraxa::util