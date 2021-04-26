#pragma once

#include <utility>

template <typename T>
struct DefaultConstructCopyableMovable {
  T val;

  DefaultConstructCopyableMovable() = default;
  DefaultConstructCopyableMovable(T val) : val(std::move(val)) {}

  DefaultConstructCopyableMovable(DefaultConstructCopyableMovable const&) : val() {}
  DefaultConstructCopyableMovable(DefaultConstructCopyableMovable&&) : val() {}

  auto& operator=(DefaultConstructCopyableMovable const&) { return *this; }
  auto& operator=(DefaultConstructCopyableMovable&&) { return *this; }
};