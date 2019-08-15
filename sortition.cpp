/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-07
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "sortition.h"

#include "libdevcore/SHA3.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <deque>
#include <iostream>
#include <locale>
#include <sstream>

namespace taraxa {
using std::string;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

string hashSignature(dev::Signature signature) {
  return dev::sha3(signature).hex();
}

/*
 * Sortition return true:
 * HASH(signature()) / SIGNATURE_HASH_MAX < account balance / TARAXA_COINS *
 * sortition_threshold otherwise return false
 */
bool sortition(string signature_hash, val_t account_balance, size_t threshold) {
  if (signature_hash.length() != 64) {
    LOG(log_error_) << "signature has string length should be 64, but "
                  << signature_hash.length() << " given" << std::endl;
    return false;
  }
  string signature_hash_decimal = taraxa::hexToDecimal(signature_hash);
  if (signature_hash_decimal.empty()) {
    LOG(log_error_) << "Cannot convert singature hash from hex to decimal";
    return false;
  }

  string sum_left;
  string sum_right;

  sum_left =
      taraxa::bigNumberMultiplication(signature_hash_decimal, TARAXA_COINS);
  if (sum_left.empty()) {
    LOG(log_error_) << "Failed multiplication of signature hash * total coins";
    return false;
  }
  val_t sum = account_balance * threshold;
  sum_right =
      taraxa::bigNumberMultiplication(SIGNATURE_HASH_MAX, toString(sum));
  if (sum_right.empty()) {
    LOG(log_error_)
        << "Failed multiplication of "
        << "max signature hash * account balance * sortition threshold";
    return false;
  }

  if (sum_left.length() < sum_right.length()) {
    return true;
  } else if (sum_left.length() > sum_right.length()) {
    return false;
  } else {
    return sum_left < sum_right;
  }
}

string hexToDecimal(string hex) {
  for (int i = 0; i < hex.length(); i++) {
    if (!std::isxdigit(hex[i])) {
      LOG(log_error_) << "invalid hex character: " << hex[i] << std::endl;
      return "";
    }
  }

  uint256_t n;
  std::stringstream ss;
  ss << std::hex << hex;
  ss >> n;
  std::stringstream decimal;
  decimal << n;
  return decimal.str();
}

string bigNumberMultiplication(string num1, string num2) {
  std::stringstream result;
  if (num1.length() > SIGNATURE_HASH_SIZE_MAX ||
      num2.length() > SIGNATURE_HASH_SIZE_MAX) {
    LOG(log_error_)
        << "The length of the input decimal strings cannot larger than 78, "
        << "the length of num1: " << num1.length()
        << ", and the length of num2: " << num2.length() << std::endl;
    return result.str();
  }
  for (char n : num1) {
    if (!isdigit(n)) {
      LOG(log_error_) << "invalid decimal digit: " << n << std::endl;
      return result.str();
    }
  }
  for (char n : num2) {
    if (!isdigit(n)) {
      LOG(log_error_) << "invalid decimal digit: " << n << std::endl;
      return result.str();
    }
  }

  uint512_t n1(num1);
  uint512_t n2(num2);
  uint512_t product = n1 * n2;
  result << std::dec << product;

  return result.str();
}

}  // namespace taraxa