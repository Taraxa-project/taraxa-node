#pragma once

#include "types.h"
#include "Hash.h"
#include "RSWPuzzle.h"
#include <cmath>
#include "util.h"

#include <iostream>
#include <chrono>
#include <vector>

namespace vdf
{
class VerifierPietrzak
{
public:
  using solution = Solution<std::vector<bytevec>>;

  VerifierPietrzak(
      const unsigned long _lambda,
      const unsigned long _t,
      const bytevec &_x,
      const unsigned long _lambdaRSW,
      const unsigned int _key_size = 256 / Hash::divisor,
      const unsigned int _block_size = 128 / Hash::divisor);
  VerifierPietrzak(
      const unsigned long _lambda,
      const unsigned long _t,
      const bytevec &_x,
      const bytevec &N,
      const unsigned int _key_size = 256 / Hash::divisor,
      const unsigned int _block_size = 128 / Hash::divisor);
  ~VerifierPietrzak() = default;

  Hash get_Hash() const;
  RSWPuzzle get_RSWPuzzle() const;
  bool operator()(const solution &sol) const;

  mutable std::vector<typename std::chrono::high_resolution_clock::duration> durations;

private:
  const Hash hash;
  const RSWPuzzle puzzle;
  BN_CTX_free_ptr ctx_ptr;
};
} // namespace vdf
