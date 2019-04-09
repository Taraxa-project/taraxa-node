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
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;
using uint1024_t = boost::multiprecision::uint1024_t;

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
  if (sum_left.empty()) {
    return false;
  }
  uint64_t sum = account_balance * THRESHOLD;
  sum_right = taraxa::bigNumberMultiplication(SIGNATURE_HASH_MAX, std::to_string(sum));
  if (sum_right.empty()) {
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
      std::cerr << "invalid hex character: " << hex[i] << std::endl;
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
  if (num1.length() > SIGNATURE_HASH_SIZE_MAX || num2.length() > SIGNATURE_HASH_SIZE_MAX) {
    std::cerr << "The length of the input decimal strings cannot larger than 78, "
              << "the length of num1: " << num1.length()
              << ", and the length of num2: " << num2.length() << std::endl;
    return result.str();
  }
  for (char n: num1) {
    if (!isdigit(n)) {
      std::cerr << "invalid decimal digit: " << n << std::endl;
      return result.str();
    }
  }
  for (char n: num2) {
    if (!isdigit(n)) {
      std::cerr << "invalid decimal digit: " << n << std::endl;
      return result.str();
    }
  }

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

/*
  uint256_t n1;
  uint256_t n2;
  std::stringstream ss;
  ss << std::dec << num1;
  ss >> n1;
  std::cout << n1 << std::endl;
  ss.clear();
  ss << std::dec << num2;
  ss >> n2;
  std::cout << n2 << std::endl;
  uint1024_t product;
  product = n1 * n2;
  std::cout << product << std::endl;
  result << std::dec << product;
  return result.str();
*/
}

} // namespace taraxa