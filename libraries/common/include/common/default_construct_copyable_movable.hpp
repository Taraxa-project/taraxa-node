#pragma once

#include <utility>

// A wrapper for noncopyable/nonmovable types that resorts to default-constructing in copy/move constructors
// and immutability in assignment operators
// This basically captures a common work-around pattern for classes that contain fields of this kind of types
// - a famous example would be a mutex type

namespace taraxa::util {

template <typename T>
struct DefaultConstructCopyableMovable {
  T val;

  DefaultConstructCopyableMovable() = default;
  ~DefaultConstructCopyableMovable() = default;

  DefaultConstructCopyableMovable(T val) : val(std::move(val)) {}
  DefaultConstructCopyableMovable(const DefaultConstructCopyableMovable&) noexcept(noexcept(T())) : val() {}
  DefaultConstructCopyableMovable(DefaultConstructCopyableMovable&&) noexcept(noexcept(T())) : val() {}

  DefaultConstructCopyableMovable& operator=(const DefaultConstructCopyableMovable&) noexcept { return *this; }
  DefaultConstructCopyableMovable& operator=(DefaultConstructCopyableMovable&&) noexcept { return *this; }
};

}  // namespace taraxa::util