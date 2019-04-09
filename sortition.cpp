/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-07
 * @Last Modified by:
 * @Last Modified time:
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
using uint128_t = boost::multiprecision::uint128_t;

string hashSignature(dev::Signature signature) {
  return dev::sha3(signature).hex();
}

/*
 * Sortition return true:
 * HASH(signature) / SIGNATURE_HASH_MAX < account balance / TARAXA_COINS * THRESHOLD
 * otherwise return false
 */
bool sortition(string signature_hash, uint64_t account_balance) {
  if (signature_hash.length() != 64) {
    std::cerr << "signature has string length should be 64, but "
              << signature_hash.length()
              << " given" << std::endl;
    return false;
  }
  string signature_hash_decimal = taraxa::hexToDecimal(signature_hash);
  if (signature_hash_decimal.empty()) {
    return false;
  }

  string sum_left;
  string sum_right;

  sum_left = taraxa::bigNumberMultiplication(signature_hash_decimal, TARAXA_COINS);
  uint64_t sum = account_balance * THRESHOLD;
  sum_right = taraxa::bigNumberMultiplication(SIGNATURE_HASH_MAX, std::to_string(sum));

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
      std::cerr << "invalid hex character: " << hex[i] << std::endl;
      return "";
    }
  }

  uint128_t n;
  std::stringstream ss;
  ss << std::hex << hex;
  ss >> n;
  std::stringstream decimal;
  decimal << n;
  return decimal.str();
}

string bigNumberMultiplication(string num1, string num2) {
  std::stringstream result;
  std::deque<int> sum(num1.length() + num2.length() - 1, 0);

  for (int i = 0; i < num1.length(); ++i) {
    for (int j = 0; j < num2.length(); ++j) {
      sum[i + j] += (num1[i] - '0') * (num2[j] - '0');
    }
  }

  int carry = 0;
  // carry in reverse order
  for (int i = sum.size() - 1; i >= 0; --i) {
    int tmp = sum[i] + carry;
    sum[i] = tmp % 10;
    carry = tmp / 10;
  }
  // add carry in the head of sum if has carry
  while (carry != 0) {
    int tmp = carry % 10;
    sum.push_front(tmp);
    carry /= 10;
  }

  for (int n: sum) {
    result << n;
  }
  return result.str();
}

} // namespace taraxa