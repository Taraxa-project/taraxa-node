#pragma once

#include <openssl/bn.h>
#include <openssl/rand.h>
#include "types.h"
#include <string>

namespace vdf
{
class RSWPuzzle
{
public:
  RSWPuzzle(const unsigned long _lambda, const unsigned long _t, const bytevec &_x, const bytevec &_N);
  RSWPuzzle(const unsigned long _lambda, const unsigned long _t, const bytevec &_x, const unsigned long _lambdaRSW);
  RSWPuzzle(const RSWPuzzle &other);
  ~RSWPuzzle();

  bytevec get_N() const;
  bytevec get_x() const;
  bytevec get_T() const;
  unsigned long get_log2T() const;
  unsigned long get_lambda() const;

protected:
  RSWPuzzle(const unsigned long _lambda, const unsigned long _t, const bytevec &_x);
  BIGNUM *N;
  BIGNUM *x;
  BIGNUM *T;
  const unsigned long t; // == log2(T)  redundant but comfortable
  const unsigned long lambda;
};
} // namespace vdf
