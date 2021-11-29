#pragma once

#include <rocksdb/comparator.h>

#include "storage/db_utils.hpp"

namespace taraxa {
class UintComparator : public rocksdb::Comparator {
 public:
  ~UintComparator() {}

  UintComparator() {}

  // Three-way comparison function:
  // if a < b: negative result
  // if a > b: positive result
  // else: zero result
  virtual int Compare(const rocksdb::Slice &a, const rocksdb::Slice &b) const override {
    const auto x = FromSlice<uint64_t>(a.data());
    const auto y = FromSlice<uint64_t>(b.data());
    if (x == y) return 0;
    if (x < y) return -1;
    return 1;
  };

  // Ignore the following methods for now:
  virtual const char *Name() const override { return "UintComparator"; };

  virtual void FindShortestSeparator(std::string *, const rocksdb::Slice &) const override{};

  virtual void FindShortSuccessor(std::string *) const override{};
};

const rocksdb::Comparator *getUintComparator() {
  const static taraxa::UintComparator cmp;
  return &cmp;
}

}  // end namespace taraxa
