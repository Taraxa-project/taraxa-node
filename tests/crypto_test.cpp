#include <ProverWesolowski.h>
#include <gtest/gtest.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <openssl/bn.h>

#include <iostream>
#include <string>

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
  VrfPbftMsg msg(PbftVoteTypes::cert_vote, 2, 2, 3);
  VrfPbftSortition sortition(sk, msg);
  VrfPbftSortition sortition2(sk, msg);

  EXPECT_FALSE(sortition.calculateWeight(0, 1, 1, dev::FixedHash<64>::random()));
  EXPECT_TRUE(sortition.calculateWeight(1, 1, 1, dev::FixedHash<64>::random()));
  auto b = sortition.getRlpBytes();
  VrfPbftSortition sortition3(b);
  sortition3.verify(getVrfPublicKey(sk));
  EXPECT_EQ(sortition, sortition2);
  EXPECT_EQ(sortition, sortition3);
}

TEST_F(CryptoTest, vdf_sortition) {
  SortitionParams sortition_params(0xffff, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 3;
  blk_hash_t vdf_input = blk_hash_t(200);
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  VdfSortition vdf2(sortition_params, sk, getRlpBytes(level));
  vdf.computeVdfSolution(sortition_params, vdf_input.asBytes(), false);
  vdf2.computeVdfSolution(sortition_params, vdf_input.asBytes(), false);
  auto b = vdf.rlp();
  VdfSortition vdf3(b);
  EXPECT_NO_THROW(vdf3.verifyVdf(sortition_params, getRlpBytes(level), getVrfPublicKey(sk), vdf_input.asBytes()));
  EXPECT_EQ(vdf, vdf2);
  EXPECT_EQ(vdf, vdf3);

  SortitionParams sortition_params_no_omit_no_stale(0xffff, 5, 10, 10, 1500);
  EXPECT_FALSE(vdf.isStale(sortition_params_no_omit_no_stale));

  SortitionParams sortition_params_omit_no_stale(0xffff, 5, 10, 10, 1500);
  EXPECT_FALSE(vdf.isStale(sortition_params_omit_no_stale));

  SortitionParams sortition_params_stale(0xfff, 5, 10, 10, 1500);
  EXPECT_TRUE(vdf.isStale(sortition_params_stale));
}

TEST_F(CryptoTest, vdf_solution) {
  SortitionParams sortition_params(0xffff, 5, 10, 10, 1500);
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
      [&vdf, vdf_input, sortition_params]() { vdf.computeVdfSolution(sortition_params, vdf_input.asBytes(), false); });
  std::thread th2([&vdf2, vdf_input, sortition_params]() {
    vdf2.computeVdfSolution(sortition_params, vdf_input.asBytes(), false);
  });
  th1.join();
  th2.join();
  EXPECT_NE(vdf, vdf2);
  std::cout << vdf << std::endl;
  std::cout << vdf2 << std::endl;
}

TEST_F(CryptoTest, vdf_proof_verify) {
  SortitionParams sortition_params(255, 5, 10, 10, 1500);
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 1;
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  blk_hash_t vdf_input = blk_hash_t(200);
  const auto pk = getVrfPublicKey(sk);
  vdf.computeVdfSolution(sortition_params, vdf_input.asBytes(), false);
  EXPECT_NO_THROW(vdf.verifyVdf(sortition_params, getRlpBytes(level), pk, vdf_input.asBytes()));
  VdfSortition vdf2(sortition_params, sk, getRlpBytes(level));
  EXPECT_THROW(vdf2.verifyVdf(sortition_params, getRlpBytes(level), pk, vdf_input.asBytes()),
               VdfSortition::InvalidVdfSortition);
}

TEST_F(CryptoTest, DISABLED_compute_vdf_solution_cost_time) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  level_t level = 1;
  uint16_t threshold_upper = 0;  // diffculty == diffuclty_stale
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
    SortitionParams sortition_params(threshold_upper, difficulty_min, difficulty_max, difficulty_stale, lambda_bound);
    VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash1.asBytes(), false);
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash1 << ", lambda bits " << lambda_bound << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash2.asBytes(), false);
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash2 << ", lambda bits " << lambda_bound << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash3.asBytes(), false);
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash3 << ", lambda bits " << lambda_bound << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
  }
  // Fix difficulty, vary lambda
  uint16_t difficulty_stale = 15;
  for (uint16_t lambda = 100; lambda <= 5000; lambda += 200) {
    std::cout << "Start at lambda " << lambda << " :" << std::endl;
    SortitionParams sortition_params(threshold_upper, difficulty_min, difficulty_max, difficulty_stale, lambda);
    VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash1.asBytes(), false);
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash1 << ", lambda bits " << lambda << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash2.asBytes(), false);
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash2 << ", lambda bits " << lambda << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
    vdf.computeVdfSolution(sortition_params, proposal_dag_block_pivot_hash3.asBytes(), false);
    vdf_computation_time = vdf.getComputationTime();
    std::cout << "VDF message " << proposal_dag_block_pivot_hash3 << ", lambda bits " << lambda << ", difficulty "
              << vdf.getDifficulty() << ", computation cost time " << vdf_computation_time << "(ms)" << std::endl;
  }

  SortitionParams sortition_params(32768, 16, 21, 22, 100);
  VdfSortition vdf(sortition_params, sk, getRlpBytes(level));
  std::cout << "output " << vdf.output_ << std::endl;
  int i = 0;
  for (; i < vdf.output_.size; i++) {
    std::cout << vdf.output_[i] << ", " << vdf.output_.hex()[i] << ", " << uint16_t(vdf.output_[i]) << std::endl;
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
    const uint256_t kVrfOutput = dev::FixedHash<32>::random();
    hitcount += VrfPbftSortition::getBinominalDistribution(kMyMoney, kTotalMoney, kExpectedSize, kVrfOutput);
  }
  const auto expected = N * kExpectedSize / 2;
  uint64_t d;
  if (expected > hitcount) {
    d = expected - hitcount;
  } else {
    d = hitcount - expected;
  }
  // within 4% good enough
  auto maxd = expected / 25;
  EXPECT_LE(d, maxd);
  std::cout << "wanted " << expected << " selections but got " << hitcount << ", d=" << d << ", maxd=" << maxd
            << std::endl;
}

TEST_F(CryptoTest, sortition_rate) {
  vrf_sk_t sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  int count = 0;
  PbftRound round = 1000;
  int valid_sortition_players = 100;
  int sortition_threshold = 5;
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 5%
  PbftStep pbft_step = 3;
  for (uint32_t i = 0; i < round; i++) {
    VrfPbftMsg msg(PbftVoteTypes::cert_vote, i, i, pbft_step);
    VrfPbftSortition sortition(sk, msg);
    count += sortition.calculateWeight(1, valid_sortition_players, sortition_threshold, sk);
  }
  EXPECT_EQ(count, 42);  // Test experience

  count = 0;
  sortition_threshold = valid_sortition_players;
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 100%
  for (uint32_t i = 0; i < round; i++) {
    VrfPbftMsg msg(PbftVoteTypes::cert_vote, i, i, pbft_step);
    VrfPbftSortition sortition(sk, msg);
    count += sortition.calculateWeight(1, valid_sortition_players, sortition_threshold, dev::FixedHash<64>::random());
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
    for (uint32_t j = 0; j < round; j++) {
      auto [pk, sk] = getVrfKeyPair();
      VrfPbftMsg msg(PbftVoteTypes::cert_vote, i, i, pbft_step);
      VrfPbftSortition sortition(sk, msg);
      count += sortition.calculateWeight(1, valid_sortition_players, sortition_threshold, dev::FixedHash<64>::random());
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
    const uint256_t k_vrf_output = dev::FixedHash<32>::random();
    auto total_count = k_total_count_at_start + i;
    auto threshold = std::min(k_committee_size, total_count);
    EXPECT_EQ(VrfPbftSortition::getBinominalDistribution(i, total_count, threshold, k_vrf_output),
              VrfPbftSortition::getBinominalDistribution(i, total_count, threshold, k_vrf_output));
  }
}

TEST_F(CryptoTest, leader_selection) {
  std::unordered_map<uint64_t, vrf_sk_t> low_stake_nodes;
  std::unordered_map<uint64_t, vrf_sk_t> high_stake_nodes;
  std::map<uint64_t, uint64_t> block_produced;

  std::srand(std::time(nullptr));
  const uint64_t committee_size = 20;
  const uint64_t rounds = 1000;
  const uint64_t low_stake_nodes_num = 20;
  const uint64_t high_stake_nodes_num = 1;
  const uint64_t high_stake_nodes_power = std::rand() % 1000;
  const uint64_t low_stake_nodes_power = 10 + std::rand() % 50;
  const auto valid_sortition_players =
      high_stake_nodes_num * high_stake_nodes_power + low_stake_nodes_num * low_stake_nodes_power;

  for (uint64_t i = 0; i < high_stake_nodes_num; i++) {
    high_stake_nodes.emplace(i, getVrfKeyPair().second);
  }
  for (uint64_t i = 0; i < low_stake_nodes_num; i++) {
    low_stake_nodes.emplace(i + high_stake_nodes_num, getVrfKeyPair().second);
  }

  const auto selector = [&](auto& outputs, const auto& msg, const auto& nodes, auto node_power) {
    for (const auto& n : nodes) {
      VrfPbftSortition sortition(n.second, msg);
      auto hash = getVoterIndexHash(sortition.output_, n.second, 0);
      if (auto stake =
              VrfPbftSortition::getBinominalDistribution(node_power, valid_sortition_players, committee_size, hash)) {
        auto lowest = getVoterIndexHash(sortition.output_, n.second, 1);
        for (uint64_t j = 2; j <= stake; j++) {
          auto tmp = getVoterIndexHash(sortition.output_, n.second, j);
          if (tmp < lowest) lowest = tmp;
        }
        outputs.emplace(n.first, lowest);
      }
    }
  };

  for (uint64_t i = 0; i < rounds; i++) {
    const VrfPbftMsg msg(PbftVoteTypes::propose_vote, i, i, 1);
    std::unordered_map<uint64_t, dev::h256> outputs;

    selector(outputs, msg, high_stake_nodes, high_stake_nodes_power);
    selector(outputs, msg, low_stake_nodes, low_stake_nodes_power);

    // every round there needs to be a leader
    EXPECT_TRUE(outputs.size());
    const auto leader = *std::min_element(outputs.begin(), outputs.end(),
                                          [](const auto& i, const auto& j) { return i.second < j.second; });
    block_produced[leader.first]++;
  }

  uint64_t high_stake_nodes_blocks = 0;
  for (const auto& node : block_produced) {
    if (node.first == high_stake_nodes_num) break;
    high_stake_nodes_blocks += node.second;
  }

  const auto stake_ratio = high_stake_nodes_power * high_stake_nodes_num * 100 / valid_sortition_players;
  const auto blocks_ratio = high_stake_nodes_blocks * 100 / rounds;
  std::cout << "High stake: " << high_stake_nodes_power << " low stake: " << low_stake_nodes_power
            << " total stake: " << valid_sortition_players << std::endl;
  std::cout << "Stake ratio: " << stake_ratio << " Blocks ratio:" << blocks_ratio << std::endl;
  const auto diff = (stake_ratio > blocks_ratio) ? (stake_ratio - blocks_ratio) : (blocks_ratio - stake_ratio);
  // maximal difference is -/+5%
  EXPECT_LE(diff, 5);
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
