#pragma once

#include <openssl/bn.h>
#include <cmath>
#include "VerifierPietrzak.h"
#include <chrono>
#include <vector>

namespace vdf
{
class ProverPietrzak
{
public:
  using solution = typename VerifierPietrzak::solution;

  ProverPietrzak();
  ~ProverPietrzak() = default;

  solution operator()(const VerifierPietrzak &verifier, long _d_max = -1l) const;

  mutable std::vector<typename std::chrono::high_resolution_clock::duration> durations;

private:
  BN_CTX_free_ptr ctx_ptr;
};
} // namespace vdf
