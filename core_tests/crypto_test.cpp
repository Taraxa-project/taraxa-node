/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-05
 * @Last Modified by:
 * @Last Modified time:
 */

#include "libdevcore/FixedHash.h"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"
#include "sortition.h"

#include <gtest/gtest.h>
#include <string>
#include <iostream>

namespace taraxa {
using std::string;

TEST(EthereumCrypto, keypair_signature_verify_hash_test) {
  dev::KeyPair key_pair = dev::KeyPair::create();
  EXPECT_EQ(key_pair.pub().size, 64);
  EXPECT_EQ(key_pair.secret().size, 32);

  string message = "0123456789abcdef";
  dev::Signature signature = dev::sign(key_pair.secret(), dev::sha3(message));
  EXPECT_EQ(signature.size, 65);

  bool verify = dev::verify(key_pair.pub(), signature, dev::sha3(message));
  EXPECT_EQ(verify, true);

  string sign_hash = taraxa::hashSignature(signature);
  EXPECT_EQ(sign_hash.length(), 64);
}

TEST(EthereumCrypto, hex_to_decimal_test) {
  string hex = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
  string hex_decimal = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
  string decimal = taraxa::hexToDecimal(hex);
  EXPECT_EQ(decimal, hex_decimal);
}

TEST(EthereumCrypto, big_number_multiplication_test) {
  // input num is the decimal of the max hash number 64 F
  string num = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
  string output = "13407807929942597099574024998205846127479365820592393377723561443721764030073315392623399665776056285720014482370779510884422601683867654778417822746804225";
  string sum = taraxa::bigNumberMultiplication(num, num);
  EXPECT_EQ(sum, output);
}

TEST(EthereumCrypto, sortition_test) {
  string signature_hash = "0000000000000000000000000000000000000000000000000000000000000011";
  uint64_t account_balance = 1000;
  bool sortition = taraxa::sortition(signature_hash, account_balance);
  EXPECT_EQ(sortition, true);
}

} // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}