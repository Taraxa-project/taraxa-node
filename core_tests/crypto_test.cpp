#include "sortition.h"

#include "full_node.hpp"
#include "libdevcore/FixedHash.h"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"
#include "pbft_manager.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include "core_tests/util.hpp"

namespace taraxa {
using namespace core_tests::util;
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
  string hex =
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
  string hex_decimal =
      "115792089237316195423570985008687907853269984665640564039457584007913129"
      "639935";
  string decimal = taraxa::hexToDecimal(hex);
  EXPECT_EQ(decimal, hex_decimal);
}

TEST(EthereumCrypto, big_number_multiplication_test) {
  // input num is the decimal of the max hash number 64 F
  string num =
      "115792089237316195423570985008687907853269984665640564039457584007913129"
      "639935";
  string output =
      "134078079299425970995740249982058461274793658205923933777235614437217640"
      "300733153926233996657760562857200144823707795108844226016838676547784178"
      "22746804225";
  string sum = taraxa::bigNumberMultiplication(num, num);
  EXPECT_EQ(sum, output);
}

TEST(EthereumCrypto, sortition_test) {
  string signature_hash =
      "0000000000000000000000000000000000000000000000000000000000000001";
  uint64_t account_balance = 1000;
  size_t sortition_threshold = 1;
  bool sortition =
      taraxa::sortition(signature_hash, account_balance, sortition_threshold);
  EXPECT_EQ(sortition, true);
}

TEST(EthereumCrypto, sortition_rate) {
  uint64_t total_coins = 9007199254740991;
  uint64_t number_of_players = 100;
  uint64_t account_balance = total_coins / number_of_players;
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  cfg.genesis_state.accounts[addr(cfg.node_secret)] = {account_balance};
  boost::asio::io_context context;
  auto node(std::make_shared<FullNode>(context, cfg, true));
  string message = "This is a test message.";
  int count = 0;
  int round = 1000;
  int sortition_threshold;
  if (node->getPbftManager()->COMMITTEE_SIZE <= number_of_players) {
    sortition_threshold = node->getPbftManager()->COMMITTEE_SIZE;
  } else {
    sortition_threshold = number_of_players;
  }
  // Test for one player sign round messages to get sortition
  for (int i = 0; i < round; i++) {
    message += std::to_string(i);
    sig_t signature = node->signMessage(message);
    vote_hash_t sig_hash = dev::sha3(signature);
    bool win =
        sortition(sig_hash.toString(), account_balance, sortition_threshold);
    if (win) {
      count++;
    }
  }
  // depend on sortition THRESHOLD, sortition rate: THRESHOLD /
  // number_of_players count should be close to sortition rate * round
  EXPECT_GT(count, 0);

  count = 0;
  round = 10;
  // Test for number of players sign message to get sortition,
  // Each player sign round messages, sortition rate for one player: THRESHOLD /
  // number_of_players * round
  for (int i = 0; i < number_of_players; i++) {
    dev::KeyPair key_pair = dev::KeyPair::create();
    for (int j = 0; j < round; j++) {
      message += std::to_string(j);
      sig_t signature = dev::sign(key_pair.secret(), dev::sha3(message));
      vote_hash_t sig_hash = dev::sha3(signature);
      bool win =
          sortition(sig_hash.toString(), account_balance, sortition_threshold);
      if (win) {
        count++;
      }
    }
  }
  // depend on sortition THRESHOLD, sortition rate for all players: THRESHOLD /
  // number_of_players * round * number_of_players count should be close to
  // sortition rate
  EXPECT_GT(count, 0);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("SORTITION");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}