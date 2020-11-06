/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-07
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "sortition.hpp"

#include <libdevcore/SHA3.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <deque>
#include <iostream>
#include <locale>
#include <sstream>

namespace taraxa {
using std::string;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

string hashSignature(dev::Signature signature) { return dev::sha3(signature).hex(); }

static boost::log::sources::severity_channel_logger<> log_error_{taraxa::createTaraxaLogger(taraxa::Verbosity::VerbosityError, "SORTI", addr_t())};

/*
 * Sortition return true:
 * CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
 * i.e., CREDENTIAL * VALID PLAYERS <= SORTITION THRESHOLD * SIGNATURE_HASH_MAX
 * otherwise return false
 */
bool sortition(string credential, size_t valid_players, size_t threshold) {
  if (credential.length() != 64) {
    LOG(log_error_) << "Credential string length should be 64, but " << credential.length() << " given";
    return false;
  }
  string credential_decimal = taraxa::hexToDecimal(credential);
  if (credential_decimal.empty()) {
    LOG(log_error_) << "Cannot convert credential from hex to decimal";
    return false;
  }

  string sum_left;
  string sum_right;
  sum_left = taraxa::bigNumberMultiplication(credential_decimal, std::to_string(valid_players));
  if (sum_left.empty()) {
    LOG(log_error_) << "Failed multiplication of "
                    << "credential * total number of valid sortition players";
    return false;
  }

  sum_right = taraxa::bigNumberMultiplication(SIGNATURE_HASH_MAX, std::to_string(threshold));
  if (sum_right.empty()) {
    LOG(log_error_) << "Failed multiplication of max signature hash * sortition threshold";
    return false;
  }

  if (sum_left.length() < sum_right.length()) {
    return true;
  } else if (sum_left.length() > sum_right.length()) {
    return false;
  } else {
    return sum_left <= sum_right;
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
  if (num1.length() > SIGNATURE_HASH_SIZE_MAX || num2.length() > SIGNATURE_HASH_SIZE_MAX) {
    LOG(log_error_) << "The length of the input decimal strings cannot larger than 78, "
                    << "the length of num1: " << num1.length() << ", and the length of num2: " << num2.length() << std::endl;
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