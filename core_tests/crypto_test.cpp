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
  std::cout << "public key: " << key_pair.pub().hex() << std::endl;
  std::cout << "secret key: " << key_pair.secret() << std::endl;
  EXPECT_EQ(key_pair.pub().size, 64);
  EXPECT_EQ(key_pair.secret().size, 32);

  string message = "0123456789abcdef";
  dev::Signature signature = dev::sign(key_pair.secret(), dev::sha3(message));
  std::cout << "signature: " << signature.hex() << std::endl;
  EXPECT_EQ(signature.size, 65);

  bool verify = dev::verify(key_pair.pub(), signature, dev::sha3(message));
  EXPECT_EQ(verify, true);

  string sign_hash = taraxa::hashSignature(signature);
  std::cout << "sign hash: " << sign_hash << std::endl;
  EXPECT_EQ(sign_hash.length(), 64);
}

TEST(EthereumCrypto, hex_to_decimal_test) {
  string hex = "ffffffffffffffffffffffffffffffff";
  string hex_decimal = "340282366920938463463374607431768211455";
  string decimal = taraxa::hexToDecimal(hex);
  EXPECT_EQ(decimal, hex_decimal);
}

TEST(EthereumCrypto, big_number_multiplication_test) {
  string num1 = "12345";
  string num2 = "67890";
  string output = "838102050";
  string sum = taraxa::bigNumberMultiplication(num1, num2);
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