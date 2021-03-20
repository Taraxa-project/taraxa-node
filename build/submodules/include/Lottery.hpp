#pragma once

#include "VerifierPietrzak.h"
#include "VerifierWesolowski.h"
#include "ProverPietrzak.h"
#include "ProverWesolowski.h"
#include "Player.h"
#include <functional>
#include <utility>

namespace vdf
{
using PlayerNULL = std::unique_ptr<Player>;

template <typename Verifier, typename Prover>
class Lottery
{
public:
  using solution = typename Verifier::solution;

  bool submit_solution(const solution &sol);

private:
  Verifier verifier;
  int pot;
  std::function<bool(const bytevec &)> judge;
  std::pair<PlayerNULL, PlayerNULL> players;
};

} // namespace vdf