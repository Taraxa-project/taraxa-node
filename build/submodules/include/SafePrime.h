#pragma once

extern "C"
{
#include <openssl/bn.h>
}

#include <string>

namespace vdf
{
class SafePrime
{
public:
  SafePrime();
  ~SafePrime();

  void generate(const int size = 32);
  const std::string print() const;

  static void seed(const std::string buffer);

private:
  BIGNUM *value;
  std::string str;
};
} // namespace vdf
