/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-05
 * @Last Modified by:
 * @Last Modified time:
 */

#include "sortition.h"

#include "full_node.hpp"
#include "libdevcore/FixedHash.h"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"

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

TEST(EthereumCrypto, sortition_rate) {
  uint64_t total_coins = 9007199254740991;
  uint64_t number_of_players = 10000;
  uint64_t account_balance = total_coins / number_of_players;
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_taraxa1.json")));
  addr_t account_address = node->getAddress();
  node->setBalance(account_address, account_balance);
  string message = "This is a test message.";
  int count = 0;
  for (int i = 0; i < 10000; i++) {
      message += std::to_string(i);
      sig_t signature = node->signMessage(message);
      sig_hash_t sig_hash = dev::sha3(signature);
      bool win = sortition(sig_hash.toString(), account_balance);
      if (win) {
        count++;
    }
  }
  // depend on 2t+1, count should be close to 2t+1
  EXPECT_GT(count, 0);
  count = 0;
  for (int i = 0; i < 10000; i++) {
    dev::KeyPair key_pair = dev::KeyPair::create();
    sig_t signature = dev::sign(key_pair.secret(), dev::sha3(message));
    sig_hash_t sig_hash = dev::sha3(signature);
    bool win = sortition(sig_hash.toString(), account_balance);
    if (win) {
      count++;
    }
  }
  // depend on 2t+1, count should be close to 2t+1
  EXPECT_GT(count, 0);
}

} // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}