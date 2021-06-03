#pragma once

#include <utility>

// A wrapper for noncopyable/nonmovable types that resorts to default-constructing in copy/move constructors
// and immutability in assignment operators
// This basically captures a common work-around pattern for classes that contain fields of this kind of types
// - a famous example would be a mutex type

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