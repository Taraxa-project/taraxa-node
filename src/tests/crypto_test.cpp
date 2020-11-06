#include <ProverWesolowski.h>
#include <gtest/gtest.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <openssl/bn.h>

#include <iostream>
#include <string>

#include "../config.hpp"
#include "../full_node.hpp"
#include "../pbft_manager.hpp"
#include "../sortition.hpp"
#include "../static_init.hpp"
#include "../util_test/util.hpp"
#include "../vdf_sortition.hpp"
#include "../vrf_wrapper.hpp"

namespace taraxa::core_tests {
using namespace vdf;
using namespace vrf_wrapper;
using namespace vdf_sortition;

struct CryptoTest : BaseTest {};

auto node_sk_string = "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd";
auto node_sk = dev::Secret(node_sk_string, dev::Secret::ConstructFromStringType::FromHex);
auto node_key = dev::KeyPair(node_sk);

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

  std::cout << "VRF pk bytes: (" << crypto_vrf_publickeybytes() << ") " << pk << std::endl;
  std::cout << "VRF sk bytes: (" << crypto_vrf_secretkeybytes() << ") " << sk << std::endl;
  if (proof) {
    std::cout << "VRF proof bytes: (" << crypto_vrf_proofbytes() << ") " << proof.value() << std::endl;
  }
  if (output) {
    std::cout << "VRF output bytes: (" << crypto_vrf_outputbytes() << ") " << output.value() << endl;
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
  vdf_sortition::VdfConfig vdf_config(255, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  Message msg(3);
  blk_hash_t vdf_input = blk_hash_t(200);
  VdfSortition vdf(vdf_config, node_key.address(), sk, msg);
  VdfSortition vdf2(vdf_config, node_key.address(), sk, msg);
  vdf.computeVdfSolution(vdf_input.toString());
  vdf2.computeVdfSolution(vdf_input.toString());
  auto b = vdf.rlp();
  VdfSortition vdf3(node_key.address(), b);
  vdf3.verify(vdf_input.toString());
  EXPECT_EQ(vdf, vdf2);
  EXPECT_EQ(vdf, vdf3);
  std::cout << vdf << std::endl;
}

TEST_F(CryptoTest, vdf_solution) {
  vdf_sortition::VdfConfig vdf_config(255, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  vrf_sk_t sk2(
      "90f59a7ee7a392c811c5d299b557a4e09e610de7d109d6b3fcb19ab8d51c9a0d931f5e7d"
      "b07c9969e438db7e287eabbaaca49ca414f5f3a402ea6997ade40081");
  Message msg(1);
  VdfSortition vdf(vdf_config, node_key.address(), sk, msg);
  VdfSortition vdf2(vdf_config, node_key.address(), sk2, msg);
  blk_hash_t vdf_input = blk_hash_t(200);

  std::thread th1([&vdf, vdf_input]() { vdf.computeVdfSolution(vdf_input.toString()); });
  std::thread th2([&vdf2, vdf_input]() { vdf2.computeVdfSolution(vdf_input.toString()); });
  th1.join();
  th2.join();
  EXPECT_NE(vdf, vdf2);
  std::cout << vdf << std::endl;
  std::cout << vdf2 << std::endl;
}

TEST_F(CryptoTest, vdf_proof_verify) {
  vdf_sortition::VdfConfig vdf_config(255, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  Message msg(3);
  VdfSortition vdf(vdf_config, node_key.address(), sk, msg);
  blk_hash_t vdf_input = blk_hash_t(200);

  vdf.computeVdfSolution(vdf_input.toString());
  EXPECT_TRUE(vdf.verify(vdf_input.toString()));
  VdfSortition vdf2(vdf_config, node_key.address(), sk, msg);
  EXPECT_FALSE(vdf2.verify(vdf_input.toString()));
}

TEST_F(CryptoTest, DISABLED_compute_vdf_solution_cost_time) {
  // When difficulty_selection is 255, vdf_sortition.cpp getDifficulty() always
  // return difficulty_stale
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  blk_hash_t last_anchor_hash = blk_hash_t("be67f76499af842b5c8e9d22194f19c04711199726b2224854af34365d351124");
  Message msg(1);
  uint16_t difficulty_selection = 255;
  uint16_t difficulty_min = 0;
  uint16_t difficulty_max = 0;
  uint16_t lambda_bound = 1500;
  blk_hash_t proposal_dag_block_pivot_hash1 = blk_hash_t(0);
  blk_hash_t proposal_dag_block_pivot_hash2 = blk_hash_t("c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69");
  blk_hash_t proposal_dag_block_pivot_hash3 = blk_hash_t("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  unsigned long vdf_computation_time;
  // Fix lambda, vary difficulty
  for (uint16_t difficulty_stale = 0; difficulty_stale <= 20; difficulty_stale++) {
    std::cout << "Start at difficulty " << difficulty_stale << " :" << std::endl;
    vdf_sortition::VdfConfig vdf_config(difficulty_selection, difficulty_min, difficulty_max, difficulty_stale, lambda_bound);
    VdfSortition vdf(vdf_config, node_key.address(), sk, msg);
    vdf.computeVdfSolution(proposal_dag_block_pivot_hash1.toString());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash1 << ", lambda bits " << vdf.getLambda() << ", difficulty " << vdf.getDifficulty()
              << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(proposal_dag_block_pivot_hash2.toString());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash2 << ", lambda bits " << vdf.getLambda() << ", difficulty " << vdf.getDifficulty()
              << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(proposal_dag_block_pivot_hash3.toString());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash3 << ", lambda bits " << vdf.getLambda() << ", difficulty " << vdf.getDifficulty()
              << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
  }
  // Fix difficulty, vary lambda
  uint16_t difficulty_stale = 15;
  for (uint16_t lambda = 100; lambda <= 5000; lambda += 200) {
    std::cout << "Start at lambda " << lambda << " :" << std::endl;
    vdf_sortition::VdfConfig vdf_config(difficulty_selection, difficulty_min, difficulty_max, difficulty_stale, lambda);
    VdfSortition vdf(vdf_config, node_key.address(), sk, msg);
    vdf.computeVdfSolution(proposal_dag_block_pivot_hash1.toString());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash1 << ", lambda bits " << vdf.getLambda() << ", difficulty " << vdf.getDifficulty()
              << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(proposal_dag_block_pivot_hash2.toString());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash2 << ", lambda bits " << vdf.getLambda() << ", difficulty " << vdf.getDifficulty()
              << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(proposal_dag_block_pivot_hash3.toString());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash3 << ", lambda bits " << vdf.getLambda() << ", difficulty " << vdf.getDifficulty()
              << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
  }

  vdf_sortition::VdfConfig vdf_config(128, 15, 21, 22, 1500);
  VdfSortition vdf(vdf_config, node_key.address(), sk, msg);
  std::cout << "output " << vdf.output << std::endl;
  int i = 0;
  for (; i < vdf.output.size; i++) {
    std::cout << vdf.output[i] << ", " << vdf.output.hex()[i] << ", " << uint16_t(vdf.output[i]) << std::endl;
  }
  std::cout << "size: " << i << std::endl;
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
  string hex = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
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
  string credential = "0000000000000000000000000000000000000000000000000000000000000001";
  size_t valid_sortition_players = 10;
  size_t sortition_threshold = 10;
  bool sortition = taraxa::sortition(credential, valid_sortition_players, sortition_threshold);
  EXPECT_EQ(sortition, true);
}

TEST_F(CryptoTest, sortition_rate) {
  auto node_cfgs = make_node_cfgs(1);
  FullNode::Handle node(node_cfgs[0]);

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
    bool win = sortition(credential.toString(), valid_sortition_players, sortition_threshold);
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
    bool win = sortition(credential.toString(), valid_sortition_players, sortition_threshold);
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
      bool win = sortition(credential.toString(), valid_sortition_players, sortition_threshold);
      if (win) {
        count++;
      }
    }
  }
  // depend on sortition THRESHOLD, sortition rate for all players:
  // THRESHOLD / VALID PLAYERS = 100%
  EXPECT_EQ(count, valid_sortition_players * round);
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();
  LoggingConfig logging;
  logging.verbosity = taraxa::VerbosityError;
  logging.channels["SORTITION"] = taraxa::VerbosityError;
  addr_t node_addr;
  setupLoggingConfiguration(node_addr, logging);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}