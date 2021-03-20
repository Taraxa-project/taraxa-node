#pragma once

#include <gmpxx.h>
extern "C"
{
#include <mpfr.h>
#include <openssl/bn.h>
}
#include "types.h"
namespace vdf
{
class Hash2Prime
{
public:
  Hash2Prime(const int &lambda);
  ~Hash2Prime() = default;

  void operator()(const BIGNUM *in, BIGNUM *out);

private:
  void expmLambertWm1(const int &lambda);

  gmp_randclass primeGen;
  mpz_class max_int, candidate, sign, seed;
  bytevec cache;
};
} // namespace vdf
