#include <gtest/gtest.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <iostream>
#include <string>

#include "ProverWesolowski.h"
#include "core_tests/util.hpp"
#include "full_node.hpp"
#include "openssl/bn.h"
#include "pbft_manager.hpp"
#include "sortition.hpp"
#include "static_init.hpp"
#include "vdf_sortition.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {
using namespace core_tests::util;
using namespace vdf;
using namespace vrf_wrapper;
using namespace vdf_sortition;
using std::string;
struct CryptoTest : core_tests::util::DBUsingTest<> {};

TEST_F(CryptoTest, VerifierWesolowski) {
  BIGNUM* N_bn = BN_secure_new();
  BN_dec2bn(&N_bn,
            "428804634136293954597073942907460909605269780080770775820474895139"
            "313373884797174409549658274767664589927455921037291323722119107130"
            "788925047898023345413122083756324027226108225954520307660745040887"
            "759852389951961524084114541339969954680762018088000762978892474599"
            "30444806028055000786854607913934385778187829");

  bytevec N = vdf::bn2bytevec(N_bn);
  VerifierWesolowski verifier(20, 8, {97}, N);

  ProverWesolowski prover;
  const auto sol = prover(verifier);

  EXPECT_TRUE(verifier(sol));

  VerifierWesolowski verifier2(20, 8, {97}, N);
  EXPECT_TRUE(verifier2(sol));

  ProverWesolowski::solution zero;
  EXPECT_FALSE(verifier(zero));

  auto sol2 = sol, sol3 = sol;
  sol2.first[0]++;
  sol3.second[0]++;

  EXPECT_FALSE(verifier(sol2));
  EXPECT_FALSE(verifier(sol3));
}

TEST_F(CryptoTest, vrf_proof_verify) {
  auto [pk, sk] = getVrfKeyPair();
  auto pk2 = getVrfPublicKey(sk);
  EXPECT_EQ(pk, pk2);
  EXPECT_TRUE(isValidVrfPublicKey(pk));
  EXPECT_TRUE(isValidVrfPublicKey(pk2));
  auto msg = getRlpBytes("helloworld!");
  auto proof = getVrfProof(sk, msg);
  EXPECT_TRUE(proof);
  auto output = getVrfOutput(pk, proof.value(), msg);
  EXPECT_TRUE(output);

  std::cout << "VRF pk bytes: (" << crypto_vrf_publickeybytes() << ") " << pk
            << std::endl;
  std::cout << "VRF sk bytes: (" << crypto_vrf_secretkeybytes() << ") " << sk
            << std::endl;
  if (proof) {
    std::cout << "VRF proof bytes: (" << crypto_vrf_proofbytes() << ") "
              << proof.value() << std::endl;
  }
  if (output) {
    std::cout << "VRF output bytes: (" << crypto_vrf_outputbytes() << ") "
              << output.value() << endl;
  }
}
TEST_F(CryptoTest, vrf_valid_Key) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  auto pk = getVrfPublicKey(sk);
  std::cout << "VRF pk: " << pk << std::endl;
  EXPECT_TRUE(isValidVrfPublicKey(pk));
}

TEST_F(CryptoTest, vdf_sortition) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  VdfMsg vdf_msg(blk_hash_t(100), 3);
  blk_hash_t vdf_input = blk_hash_t(200);
  VdfSortition vdf(sk, vdf_msg, 17, 9);
  VdfSortition vdf2(sk, vdf_msg, 17, 9);
  vdf.computeVdfSolution(vdf_input.toString());
  vdf2.computeVdfSolution(vdf_input.toString());
  auto b = vdf.rlp();
  VdfSortition vdf3(b);
  vdf3.verify(vdf_input.toString());
  EXPECT_EQ(vdf, vdf2);
  EXPECT_EQ(vdf, vdf3);
  std::cout << vdf << std::endl;
}

TEST_F(CryptoTest, vdf_solution) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  vrf_sk_t sk2(
      "90f59a7ee7a392c811c5d299b557a4e09e610de7d109d6b3fcb19ab8d51c9a0d931f5e7d"
      "b07c9969e438db7e287eabbaaca49ca414f5f3a402ea6997ade40081");
  VdfMsg vdf_msg(
      blk_hash_t(
          "c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69"),
      1);
  VdfSortition vdf(sk, vdf_msg, 19, 8);
  VdfSortition vdf2(sk2, vdf_msg, 19, 8);
  blk_hash_t vdf_input = blk_hash_t(200);

  std::thread th1(
      [&vdf, vdf_input]() { vdf.computeVdfSolution(vdf_input.toString()); });
  std::thread th2(
      [&vdf2, vdf_input]() { vdf2.computeVdfSolution(vdf_input.toString()); });
  th1.join();
  th2.join();
  EXPECT_NE(vdf, vdf2);
  std::cout << vdf << std::endl;
  std::cout << vdf2 << std::endl;
}

TEST_F(CryptoTest, vdf_proof_verify) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  VdfMsg vdf_msg(blk_hash_t(100), 3);
  VdfSortition vdf(sk, vdf_msg);
  blk_hash_t vdf_input = blk_hash_t(200);

  vdf.computeVdfSolution(vdf_input.toString());
  EXPECT_TRUE(vdf.verify(vdf_input.toString()));
  VdfSortition vdf2(sk, vdf_msg);
  EXPECT_FALSE(vdf2.verify(vdf_input.toString()));
}

TEST_F(CryptoTest, vrf_sortition) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  blk_hash_t blk(123);
  VrfPbftMsg msg(blk, PbftVoteTypes::cert_vote_type, 2, 3);
  VrfPbftSortition sortition(sk, msg);
  VrfPbftSortition sortition2(sk, msg);

  EXPECT_FALSE(sortition.canSpeak(10000000, 20000000));
  EXPECT_TRUE(sortition.canSpeak(1, 1));
  auto b = sortition.getRlpBytes();
  VrfPbftSortition sortition3(b);
  EXPECT_EQ(sortition, sortition2);
  EXPECT_EQ(sortition, sortition3);
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
  sortition_threshold = node->getPbftManager()->COMMITTEE_SIZE;  // 5
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
  EXPECT_EQ(count, 54);  // Test experience

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
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("SORTITION");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}