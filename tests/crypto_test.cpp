#include <ProverWesolowski.h>
#include <gtest/gtest.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <openssl/bn.h>

#include <iostream>
#include <string>
#include <unordered_map>

#include "common/static_init.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "util_test/gtest.hpp"
#include "vdf/sortition.hpp"
#include "vote/vote.hpp"

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
  BN_clear_free(N_bn);
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
    std::cout << "VRF output bytes: (" << crypto_vrf_outputbytes() << ") " << output.value() << std::endl;
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

TEST_F(CryptoTest, vrf_sortition) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, 2, 3);
  VrfPbftSortition sortition(sk, msg);
  VrfPbftSortition sortition2(sk, msg);

  EXPECT_FALSE(sortition.getBinominalDistribution(0, 1, 1));
  EXPECT_TRUE(sortition.getBinominalDistribution(1, 1, 1));
  auto b = sortition.getRlpBytes();
  VrfPbftSortition sortition3(b);
  sortition3.verify();
  EXPECT_EQ(sortition, sortition2);
  EXPECT_EQ(sortition, sortition3);
}

TEST_F(CryptoTest, vdf_sortition) {
  SortitionParams sortition_params(0xffff, 0xffff - 0xe665, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 3;
  blk_hash_t vdf_input = blk_hash_t(200);
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  VdfSortition vdf2(sortition_params, sk, getRlpBytes(level));
  vdf.computeVdfSolution(sortition_params, vdf_input.asBytes());
  vdf2.computeVdfSolution(sortition_params, vdf_input.asBytes());
  auto b = vdf.rlp();
  VdfSortition vdf3(b);
  EXPECT_NO_THROW(vdf3.verifyVdf(sortition_params, getRlpBytes(level), vdf_input.asBytes()));
  EXPECT_EQ(vdf, vdf2);
  EXPECT_EQ(vdf, vdf3);

  SortitionParams sortition_params_no_omit_no_stale(0xffff, 0xffff, 5, 10, 10, 1500);
  EXPECT_FALSE(vdf.isStale(sortition_params_no_omit_no_stale));
  EXPECT_FALSE(vdf.isOmitVdf(sortition_params_no_omit_no_stale));

  SortitionParams sortition_params_omit_no_stale(0xffff, 0xffff - 0xff00, 5, 10, 10, 1500);
  EXPECT_FALSE(vdf.isStale(sortition_params_omit_no_stale));
  EXPECT_TRUE(vdf.isOmitVdf(sortition_params_omit_no_stale));

  SortitionParams sortition_params_stale(0xfff, 0xfff, 5, 10, 10, 1500);
  EXPECT_TRUE(vdf.isStale(sortition_params_stale));
}

TEST_F(CryptoTest, vdf_solution) {
  SortitionParams sortition_params(0xffff, 0xe665, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  vrf_sk_t sk2(
      "90f59a7ee7a392c811c5d299b557a4e09e610de7d109d6b3fcb19ab8d51c9a0d931f5e7d"
      "b07c9969e438db7e287eabbaaca49ca414f5f3a402ea6997ade40081");
  level_t level = 1;
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  VdfSortition vdf2(sortition_params, sk2, getRlpBytes(level));
  blk_hash_t vdf_input = blk_hash_t(200);

  std::thread th1(
      [&vdf, vdf_input, sortition_params]() { vdf.computeVdfSolution(sortition_params, vdf_input.asBytes()); });
  std::thread th2(
      [&vdf2, vdf_input, sortition_params]() { vdf2.computeVdfSolution(sortition_params, vdf_input.asBytes()); });
  th1.join();
  th2.join();
  EXPECT_NE(vdf, vdf2);
  std::cout << vdf << std::endl;
  std::cout << vdf2 << std::endl;
}

TEST_F(CryptoTest, vdf_proof_verify) {
  SortitionParams sortition_params(255, 0, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 1;
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  blk_hash_t vdf_input = blk_hash_t(200);

  vdf.computeVdfSolution(sortition_params, vdf_input.asBytes());
  EXPECT_NO_THROW(vdf.verifyVdf(sortition_params, getRlpBytes(level), vdf_input.asBytes()));
  VdfSortition vdf2(sortition_params, sk, getRlpBytes(level));
  EXPECT_THROW(vdf2.verifyVdf(sortition_params, getRlpBytes(level), vdf_input.asBytes()),
               VdfSortition::InvalidVdfSortition);
}

TEST_F(CryptoTest, DISABLED_compute_vdf_solution_cost_time) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 1;
  uint16_t threshold_upper = 0;  // diffculty == diffuclty_stale
  uint16_t threshold_range = 0;  // Force no omit VDF
  uint16_t difficulty_min = 0;
  uint16_t difficulty_max = 0;
  uint16_t lambda_bound = 100;
  blk_hash_t proposal_dag_block_pivot_hash1 = blk_hash_t(0);
  blk_hash_t proposal_dag_block_pivot_hash2 =
      blk_hash_t("c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69");
  blk_hash_t proposal_dag_block_pivot_hash3 =
      blk_hash_t("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  unsigned long vdf_computation_time;
  // Fix lambda, vary difficulty
  for (uint16_t difficulty_stale = 0; difficulty_stale <= 20; difficulty_stale++) {
    std::cout << "Start at difficulty " << difficulty_stale << " :" << std::endl;
    SortitionParams sortition_params(threshold_upper, threshold_range, difficulty_min, difficulty_max, difficulty_stale,
                                     lambda_bound);
    VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash1.asBytes());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash1 << ", lambda bits " << lambda_bound << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash2.asBytes());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash2 << ", lambda bits " << lambda_bound << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash3.asBytes());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash3 << ", lambda bits " << lambda_bound << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
  }
  // Fix difficulty, vary lambda
  uint16_t difficulty_stale = 15;
  for (uint16_t lambda = 100; lambda <= 5000; lambda += 200) {
    std::cout << "Start at lambda " << lambda << " :" << std::endl;
    SortitionParams sortition_params(threshold_upper, threshold_range, difficulty_min, difficulty_max, difficulty_stale,
                                     lambda);
    VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash1.asBytes());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash1 << ", lambda bits " << lambda << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash2.asBytes());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash2 << ", lambda bits " << lambda << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash3.asBytes());
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash3 << ", lambda bits " << lambda << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
  }

  SortitionParams sortition_params(32768, 29184, 16, 21, 22, 100);
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  std::cout << "output " << vdf.output << std::endl;
  int i = 0;
  for (; i < vdf.output.size; i++) {
    std::cout << vdf.output[i] << ", " << vdf.output.hex()[i] << ", " << uint16_t(vdf.output[i]) << std::endl;
  }
  std::cout << "size: " << i << std::endl;
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

  string credential = dev::sha3(signature).hex();
  EXPECT_EQ(credential.length(), 64);
}

TEST_F(CryptoTest, new_sortition_rate) {
  uint64_t hitcount = 0;
  const uint64_t N = 1000;
  const uint64_t kExpectedSize = 20;
  const uint64_t kMyMoney = 100;
  const uint64_t kTotalMoney = 200;
  for (uint64_t i = 0; i < N; i++) {
    const uint512_t kVrfOutput = dev::FixedHash<64>::random();
    hitcount += VrfPbftSortition::getBinominalDistribution(kMyMoney, kTotalMoney, kExpectedSize, kVrfOutput);
  }
  const auto expected = N * kExpectedSize / 2;
  uint64_t d;
  if (expected > hitcount) {
    d = expected - hitcount;
  } else {
    d = hitcount - expected;
  }
  // within 2.5% good enough
  auto maxd = expected / 40;
  EXPECT_LE(d, maxd);
  std::cout << "wanted " << expected << " selections but got " << hitcount << ", d=" << d << ", maxd=" << maxd
            << std::endl;
}

TEST_F(CryptoTest, sortition_rate) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  int count = 0;
  int round = 1000;
  int valid_sortition_players = 100;
  int sortition_threshold = 5;
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 5%
  size_t pbft_step = 3;
  for (int i = 0; i < round; i++) {
    VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, i, pbft_step);
    VrfPbftSortition sortition(sk, msg);
    count += sortition.getBinominalDistribution(1, valid_sortition_players, sortition_threshold);
  }
  EXPECT_EQ(count, 54);  // Test experience

  count = 0;
  sortition_threshold = valid_sortition_players;
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 100%
  for (int i = 0; i < round; i++) {
    VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, i, pbft_step);
    VrfPbftSortition sortition(sk, msg);
    count += sortition.getBinominalDistribution(1, valid_sortition_players, sortition_threshold);
  }
  // depend on sortition THRESHOLD
  // CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
  EXPECT_EQ(count, round);

  count = 0;
  round = 10;
  // Test for number of players sign message to get sortition,
  // Each player sign round messages, sortition rate for one player: THRESHOLD / PLAYERS = 100%
  for (int i = 0; i < valid_sortition_players; i++) {
    dev::KeyPair key_pair = dev::KeyPair::create();
    for (int j = 0; j < round; j++) {
      auto [pk, sk] = getVrfKeyPair();
      VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, i, pbft_step);
      VrfPbftSortition sortition(sk, msg);
      count += sortition.getBinominalDistribution(1, valid_sortition_players, sortition_threshold);
    }
  }
  // depend on sortition THRESHOLD, sortition rate for all players:
  // THRESHOLD / VALID PLAYERS = 100%
  EXPECT_EQ(count, valid_sortition_players * round);
}

TEST_F(CryptoTest, binomial_distribution) {
  const uint64_t number = 1000;
  const uint64_t k_committee_size = 1000;
  const uint64_t k_total_count_at_start = 900;
  for (uint64_t i = 1; i <= number; i++) {
    const uint512_t k_vrf_output = dev::FixedHash<64>::random();
    auto total_count = k_total_count_at_start + i;
    auto threshold = std::min(k_committee_size, total_count);
    EXPECT_EQ(VrfPbftSortition::getBinominalDistribution(i, total_count, threshold, k_vrf_output),
              VrfPbftSortition::getBinominalDistribution(i, total_count, threshold, k_vrf_output));
  }
}

TEST_F(CryptoTest, testnet) {
  std::unordered_map<uint64_t, vrf_sk_t> community;
  std::unordered_map<uint64_t, vrf_sk_t> our;
  const uint64_t committee_size = 1000;
  const uint64_t rounds = 1000;
  const uint64_t comunity_nodes = 400;
  const uint64_t our_nodes = 6;
  for (uint64_t i = 0; i < our_nodes; i++) {
    our.emplace(i, getVrfKeyPair().second);
  }
  for (uint64_t i = 0; i < comunity_nodes; i++) {
    community.emplace(i + our_nodes, getVrfKeyPair().second);
  }
  const uint64_t our_nodes_power = 1700;
  const uint64_t comunity_nodes_power = 10;
  const auto valid_sortition_players = our_nodes * our_nodes_power + comunity_nodes * comunity_nodes_power;

  uint64_t total_our_nodes_power = 0;
  uint64_t total_comunity_nodes_power = 0;
  std::map<uint64_t, uint64_t> block_produced;
  for (uint64_t i = 0; i < rounds; i++) {
    const VrfPbftMsg msg(propose_vote_type, i, 1);
    std::unordered_map<uint64_t, uint512_t> outputs;
    for (const auto& n : our) {
      VrfPbftSortition sortition(n.second, msg);
      if (sortition.getBinominalDistribution(our_nodes_power, valid_sortition_players, committee_size)) {
        total_our_nodes_power += our_nodes_power;
        outputs.emplace(n.first, sortition.output);
      }
    }
    for (const auto& n : community) {
      VrfPbftSortition sortition(n.second, msg);
      if (sortition.getBinominalDistribution(comunity_nodes_power, valid_sortition_players, committee_size)) {
        total_comunity_nodes_power += comunity_nodes_power;
        outputs.emplace(n.first, sortition.output);
        // std::cout << "Node " << n.first << " propose in round " << i << std::endl;
      }
    }
    const auto leader = *std::min_element(outputs.begin(), outputs.end(),
                                          [](const auto& i, const auto& j) { return i.second < j.second; });
    block_produced[leader.first]++;
  }

  uint64_t our_blocks = 0;
  uint64_t total_blocks = 0;
  for (const auto& node : block_produced) {
    if (node.first < 6) our_blocks += node.second;
    total_blocks += node.second;
    // std::cout << "Node " << node.first << " produced " << node.second << " blocks" << std::endl;
  }
  std::cout << "Total ratio: 0."
            << our_nodes_power * our_nodes * 1000 /
                   (our_nodes_power * our_nodes + comunity_nodes * comunity_nodes_power)
            << std::endl;
  std::cout << "After selection ratio: 0."
            << total_our_nodes_power * 1000 / (total_our_nodes_power + total_comunity_nodes_power) << std::endl;
  std::cout << "Blocks ratio: 0." << our_blocks * 1000 / (total_blocks) << std::endl;
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();

  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;
  logging.channels["SORTITION"] = logger::Verbosity::Error;
  addr_t node_addr;
  logging.InitLogging(node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
