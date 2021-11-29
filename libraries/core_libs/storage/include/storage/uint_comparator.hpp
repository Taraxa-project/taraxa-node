#pragma once

#include <rocksdb/comparator.h>

#include "storage/db_utils.hpp"

namespace taraxa {
template <class T>
class UintComparator : public rocksdb::Comparator {
 public:
  UintComparator() = default;
  ~UintComparator() = default;

  static const char *kClassName() { return "taraxa.UintComparator"; }
  virtual const char *Name() const override { return kClassName(); }

  // Three-way comparison function:
  // if a < b: negative result
  // if a > b: positive result
  // else: zero result
  virtual int Compare(const rocksdb::Slice &a, const rocksdb::Slice &b) const override {
    const auto x = FromSlice<T>(a.data());
    const auto y = FromSlice<T>(b.data());
    if (x == y) return 0;
    if (x < y) return -1;
    return 1;
  };

  virtual void FindShortestSeparator(std::string *, const rocksdb::Slice &) const override{};

  virtual void FindShortSuccessor(std::string *) const override{};
};

template <class T>
inline const rocksdb::Comparator *getIntComparator() {
  const static taraxa::UintComparator<T> kComparator;
  return &kComparator;
}
}  // end namespace taraxa
