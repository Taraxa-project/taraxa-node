#pragma once

extern "C"
{
#include <openssl/bn.h>
}

#include <string>

namespace vdf
{
class BigNum
{
public:
  BigNum();
  BigNum(const BigNum &rhs);
  ~BigNum();

  BigNum &operator=(const BigNum &rhs);
  const BigNum operator+(const BigNum &rhs) const;
  const BigNum operator-(const BigNum &rhs) const;
  const BigNum operator*(const BigNum &rhs) const;
  const BigNum operator/(const BigNum &rhs) const;
  const BigNum operator%(const BigNum &rhs) const;
  BigNum &operator+=(const BigNum &rhs);
  BigNum &operator-=(const BigNum &rhs);
  BigNum &operator*=(const BigNum &rhs);
  BigNum &operator/=(const BigNum &rhs);
  BigNum &operator%=(const BigNum &rhs);
  bool operator==(const BigNum &rhs) const;
  bool operator!=(const BigNum &rhs) const;
  void set(const int n);
  void set(const std::string s);
  std::string print() const;

private:
  BIGNUM *value;
};
} // namespace vdf
