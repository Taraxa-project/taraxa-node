#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include "ProverWesolowski.h"
#include "core_tests/util.hpp"
#include "full_node.hpp"
#include "libdevcore/FixedHash.h"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"
#include "openssl/bn.h"
#include "pbft_manager.hpp"
#include "sodium.h"
#include "sortition.h"

namespace taraxa {
using namespace core_tests::util;
using namespace vdf;
using std::string;
struct CryptoTest : core_tests::util::DBUsingTest<> {};

TEST_F(CryptoTest, VerifierWesolowski) {
  BIGNUM* N_bn = BN_secure_new();
  BN_dec2bn(&N_bn, "10");  // 799979478482341
  bytevec N = vdf::bn2bytevec(N_bn);

  VerifierWesolowski verifier(20, 8, {97}, N);
  ProverWesolowski prover;
  const auto sol = prover(verifier);
  auto ok = verifier(sol);
  EXPECT_TRUE(ok);
}

TEST_F(CryptoTest, vrf_proof_verify) {
  std::string sk(crypto_vrf_SECRETKEYBYTES, ' '),
      pk(crypto_vrf_PUBLICKEYBYTES, ' ');
  crypto_vrf_keypair((unsigned char*)pk.data(), (unsigned char*)sk.data());
  std::string message("helloworld");
  crypto_vrf_keypair((unsigned char*)pk.data(), (unsigned char*)sk.data());
  std::cout << "VRF pk bytes: (" << crypto_vrf_publickeybytes() << ") "
            << toHex(pk) << std::endl;
  std::cout << "VRF sk bytes: (" << crypto_vrf_secretkeybytes() << ") "
            << toHex(sk) << std::endl;
  string proof(crypto_vrf_proofbytes(), ' ');
  crypto_vrf_prove((unsigned char*)proof.data(),
                   (const unsigned char*)sk.data(),
                   (const unsigned char*)message.data(), message.size());

  std::cout << "VRF proof bytes: (" << crypto_vrf_proofbytes() << ") "
            << toHex(proof) << std::endl;

  string output(crypto_vrf_outputbytes(), ' ');

  auto res = crypto_vrf_verify(
      (unsigned char*)output.data(), (const unsigned char*)pk.data(),
      (const unsigned char*)proof.data(), (const unsigned char*)message.data(),
      message.size());
  std::cout << "VRF output bytes: (" << crypto_vrf_outputbytes() << ") "
            << toHex(output) << endl;
  EXPECT_FALSE(res);  // means success
}

TEST_F(CryptoTest, keypair_signature_verify_hash_test) {
  dev::KeyPair key_pair = dev::KeyPair::create();
  EXPECT_EQ(key_pair.pub().size, 64);
  EXPECT_EQ(key_pair.secret().size, 32);

  string message = "0123456789abcdef";
  dev::Signature signature = dev::sign(key_pair.secret(), dev::sha3(message));
  EXPECT_EQ(signature.size, 65);

  bool verify = dev::verify(key_pair.pub(), signature, dev::sha3(message));
  EXPECT_EQ(verify, true);

  string credential = taraxa::hashSignature(signature);
  EXPECT_EQ(credential.length(), 64);
}

TEST_F(CryptoTest, hex_to_decimal_test) {
  string hex =
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
  string hex_decimal =
      "115792089237316195423570985008687907853269984665640564039457584007913129"
      "639935";
  string decimal = taraxa::hexToDecimal(hex);
  EXPECT_EQ(decimal, hex_decimal);
}

TEST_F(CryptoTest, big_number_multiplication_test) {
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

TEST_F(CryptoTest, sortition_test) {
  string credential =
      "0000000000000000000000000000000000000000000000000000000000000001";
  size_t valid_sortition_players = 10;
  size_t sortition_threshold = 10;
  bool sortition = taraxa::sortition(credential, valid_sortition_players,
                                     sortition_threshold);
  EXPECT_EQ(sortition, true);
}

TEST_F(CryptoTest, sortition_rate) {
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  auto node(FullNode::make(cfg, true));

  size_t valid_sortition_players;
  string message = "This is a test message.";
  int count = 0;
  int round = 1000;
  int sortition_threshold;

  valid_sortition_players = 100;
  sortition_threshold = node->getPbftManager()->COMMITTEE_SIZE; // 5
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 5%
  for (int i = 0; i < round; i++) {
    message += std::to_string(i);
    sig_t signature = node->signMessage(message);
    sig_hash_t credential = dev::sha3(signature);
    bool win = sortition(credential.toString(), valid_sortition_players,
                         sortition_threshold);
    if (win) {
      count++;
    }
  }
  EXPECT_EQ(count, 54); // Test experience

  count = 0;
  valid_sortition_players = node->getPbftManager()->COMMITTEE_SIZE;
  if (node->getPbftManager()->COMMITTEE_SIZE <= valid_sortition_players) {
    sortition_threshold = node->getPbftManager()->COMMITTEE_SIZE;
  } else {
    sortition_threshold = valid_sortition_players;
  }
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 100%
  for (int i = 0; i < round; i++) {
    message += std::to_string(i);
    sig_t signature = node->signMessage(message);
    sig_hash_t credential = dev::sha3(signature);
    bool win = sortition(credential.toString(), valid_sortition_players,
                         sortition_threshold);
    if (win) {
      count++;
    }
  }
  // depend on sortition THRESHOLD
  // CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
  EXPECT_EQ(count, round);

  count = 0;
  round = 10;
  // Test for number of players sign message to get sortition,
  // Each player sign round messages, sortition rate for one player:
  // THRESHOLD / PLAYERS = 100%
  for (int i = 0; i < valid_sortition_players; i++) {
    dev::KeyPair key_pair = dev::KeyPair::create();
    for (int j = 0; j < round; j++) {
      message += std::to_string(j);
      sig_t signature = dev::sign(key_pair.secret(), dev::sha3(message));
      sig_hash_t credential = dev::sha3(signature);
      bool win = sortition(credential.toString(), valid_sortition_players,
                           sortition_threshold);
      if (win) {
        count++;
      }
    }
  }
  // depend on sortition THRESHOLD, sortition rate for all players:
  // THRESHOLD / VALID PLAYERS = 100%
  EXPECT_EQ(count, valid_sortition_players * round);
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