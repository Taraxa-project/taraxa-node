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
  VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, 2, 3);
  VrfPbftSortition sortition(sk, msg);
  VrfPbftSortition sortition2(sk, msg);

  EXPECT_FALSE(sortition.calculateWeight(0, 1, 1, dev::FixedHash<64>::random()));
  EXPECT_TRUE(sortition.calculateWeight(1, 1, 1, dev::FixedHash<64>::random()));
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
  SortitionParams sortition_params(0xffff, 0xffff - 0xe665, 5, 10, 10, 1500);
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
  int round = 1000;
  int valid_sortition_players = 100;
  int sortition_threshold = 5;
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 5%
  size_t pbft_step = 3;
  for (int i = 0; i < round; i++) {
    VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, i, pbft_step);
    VrfPbftSortition sortition(sk, msg);
    count += sortition.calculateWeight(1, valid_sortition_players, sortition_threshold, sk);
  }
  EXPECT_EQ(count, 48);  // Test experience

  count = 0;
  sortition_threshold = valid_sortition_players;
  // Test for one player sign round messages to get sortition
  // Sortition rate THRESHOLD / PLAYERS = 100%
  for (int i = 0; i < round; i++) {
    VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, i, pbft_step);
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
    for (int j = 0; j < round; j++) {
      auto [pk, sk] = getVrfKeyPair();
      VrfPbftMsg msg(PbftVoteTypes::cert_vote_type, i, pbft_step);
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

// Initial 55 VRF secret keys
std::vector<std::string> vrf_secret_keys{
    "61ed7a7e3bb63f396a97c575fb4ff0d56bdde616ef369b0dec71b684b5219d749f22285082df4ae8dde8ffa7824b74e1c0757278f6f510663f"
    "1268a540e480d0",
    "7005a0395d6785204dc7bc79469a65deb0e9b821d188b0ab8c68f978cec224dfb7b88b4da86086cafffc66328ad509064a3589564b5ec4840e"
    "593119d0963edb",
    "200945e9f0246ad6c8d6bf5994ac7362dd5b8c3954912ec128af6108bdf788038295495daf6d0c63a36042448ecb5746b0eef53707bcbb3578"
    "747b85ce2da00a",
    "098719768d0ca919cca557023ae53b570de908a5e53bfca7bb8a6fb9337160631ecc49035ef15c83003aad28c26119c8dbf23e658b8af6f6d8"
    "de41d337773e69",
    "4f66c93d042400b3a02e688e43f857b9bbce8d85f11fa385ce1d4a21e60a5a82734eba0d6f2cb614021ba2bc05079965dd4ace7dc016609bf0"
    "4d14d708252d3a",
    "5b2be25d9fc2a357badf1d88c440575b58a82883f34bb9698fb7f185f5c348f7a0762815cf84cd82ec83576ef55712e7ee0eaadfbee32bfda5"
    "11f688ef234f4e",
    "ae9c2d5fd2c7cac9e2fd79d553bf9a885bb4e2ead23d58e8631288503055c51f83079497be9c9eeef9a56369bcc51b11c80e6346f683e45704"
    "da124c876a50a0",
    "1b7000424965291df6eace49a87af937e821775c826d267da347f19e3fd9dbd830bee495ec705234c34e90b96d362fadec4e0f17ff40206f50"
    "c6b292357eb6cc",
    "b763bc7ea02c9567306ac8481791f0ff07d6a131bfe8c343a39e92b56a7b021a884d28676c7e83b8f670bc08802e97aa3dc85f496a7e3be6a9"
    "b6311a23943821",
    "a8c4a2c472f589621ad3d247c42995e48804254758f9c2214a8a95ce89b6e3952cc9f688904105ce2d51613666e71280a6c5a968dc34f35484"
    "bf2709c62dd938",
    "c8dfef6ba488a0a9daf08058710c1a543fd939f7cce70d1817f1b8bb784cf9377b0523ab137775f0022316c7ce8726bd0760108a9e1cc60fb1"
    "4fc8fc9bebc092",
    "761c90476ad924c9927582a53499de846584bc5452618419134dc44a302357f33a94f91e2f1c9bd20cc10f49fc402470493fc31b5c2c59c055"
    "90d9603aa4878a",
    "4c3260712220d77394601ad3423b9d9f78a302a4727415b2a9ade1a4bacbb0a2fc7ddb0e864c244e391f1514f9ad4a6e10f51add995f650450"
    "2aa820d175f9ab",
    "efd2e07c0766f1b64f49a56ccee231e743bfd08ea765b8b7f89cdc156e84cde275321f4e4be8f0f64579d07fe450e7c6ede5990ce5b8920cb2"
    "c7f2b91659bbe7",
    "97bf003e22de5c56a9db6e904f4c8b0a3cb2fd5f723f213ace1afc4ac2b15d210c4fb4999e45edd11fdb5aa6969931ebd5c0dee3e689eb4f00"
    "cf014f6a06db24",
    "a4f9c4097bf844cc09f842febc860557d2be02bf24ad28f5a1480b1e8b8610c4e0e6a72d8bc2a6ef6c51bd863176c4085504c67a6d656ad8e6"
    "b097793f5daa09",
    "3b4d00b57e26dfd9f49e272333881697c7fe9d071f08900294a2c4cf99840310f41d34977b826fd2dbb7c4abef45fe6e4a9e2c7a37a4ddb8b5"
    "98644e47f1a64e",
    "28b30755c8ddcff0b0aff5819bab85cb54ac145db0aa748c67a1d1835652f91b8b76036bc02a0ab04d15d0c410844a845aad20024ba559a486"
    "c80dd629c3eca7",
    "b09bbfd8b9ebfe57d43635fac6a396ea91a4e72f5141b707fdf03b230a249048f46a46be34eca585a49f2f34f5de7083b1291eaca562d3f1f0"
    "7c79550aee40d4",
    "5a41ad66bc8dc9c55ce044ab981cdb95d1a6e4a100f650dc2e573a314aaef15f276559c83b8222da60b3d4fcefe908be6e0612af322d5528b8"
    "d14dd933b8ea53",
    "aa2f53f23176b55fa33e183262e6b064611e6bd3d8ca2c1b19675ffda98044729c80ce235f24593087041551d66d0bb13b5c334d0ff9173688"
    "7943a007c9f835",
    "0364ed1852b80ad4dfc306c5f174f8a7c568906610cd7ff0ddd5303d0649d73e3c72e1a9e987b86796ff1c817ccbb8ad6ca0cf6992bcfd4610"
    "7693709e0cb2f7",
    "a7934b7c6d199661ac46aadbf92366239be97096026a9625252dceb857bd43629deb3436087c51772e5d7be1ebb989e627e57fbaf7930c111b"
    "eff249ad9baa4c",
    "dcc6a2f11e61f4ae0c5ce00ba9c6d74dbec842e7e23efb724f64b7713d5482494682e369b4edda666d31bce92b3475b74840a4d713df480c67"
    "d0d6592372e1a3",
    "bad9e58c9bc3ddc83348dc583ffc57b6846924d0fd368de34e396f54869a3b853210d7a36c6bd5fa551d12f63013ea283fa9520d9383692852"
    "f3f3e6397f9040",
    "b4d5debb980cfe62f402b539cbe0d5efb59bd433d6343ac5d623d4737848f4975ae2fe255dca6685f9b94c59cd209cf1785e19940bb54160e2"
    "a27124a6817a2e",
    "d38cc77158cbc8376b0ff301c4fefe7e3160288a4244fd9813d37971bdff53b2f17554f309c2dbdc76f06d66352dcfa591b7e15beb0d1b873a"
    "49032d67fc9bd0",
    "73504535261a42e2040e8d7779f42f5bcbc41acbe91a96583d091083de5eb4be76b91e30caef4bf8a3917fa0e7698babe98d65c0102717b967"
    "cb99df7372e112",
    "5ab147ac78293da00eed74926305deebc6a4e03ba32211149eb522e9b8295a3f692a3d13a0f5d73f6cfcd811a08fe0e5fcc28ac11a6e6bbd72"
    "5737accce06c81",
    "9e3b725df0e0b630ad249872b3a46340fb7aa3cadd27b14278fe3a8a1ea92963589d1ba9e8c8ebdd04c630e09ffa7bb2e40dd9ffb202e3ded7"
    "915eeeded47aa5",
    "5b7a402d5374c3bba041283c7f81defc22d1c24ac14840eed74304469a3d665a411a9b7adad0586c1456e8ac0628379bc13a9abb28673f0337"
    "b4b85a6514274d",
    "c0113edf1e55dfb1779dac6ae2f88bbf5fac97dae94189402d463d113ab83b7f3651b85490bdb1fc36adf33ddd9217a650561b8ac5f37f65ba"
    "0b50853d69fb76",
    "d566eb774e701f34749d57efc089210d4827da632a03b3db6b47a2936c94b7b02c64cc841b8ab063d69f8268b4c1e570d70d03a40e271dc0c1"
    "b47268f6bceaf1",
    "c6fc9e2e90fa4b9cca0758d449ad9723a63c1c7ce85835bd618974f37a42d6e730d331cf0f2c19cdb858e2daae6a96933608ebcf54e34ed540"
    "e3b41b3dd4b47a",
    "7f6964f0878a59cd07cce09d67fb130618c2738a0ca6eaab8dc2fbcee134cd31daf2069bc6074675d3ddd6e4acc3f3af7c8b6a8553f652e486"
    "eb403935c91450",
    "3d41eceea93f467534c8028d9a0c85b816524717badda57301481453a661c6b2ee3e2dbf67ee08398b3eb50da6fb845ff7d020d09925ca50e8"
    "041c37178137d3",
    "cc49698c69d284af08c7b1fc67c153e1db6f1ec7288a47dc567df18ba20d0cdf01ba35ca40a8cd1c26aac28eb7019cca893d3b0f28d4b5034e"
    "46ed55a2cb9cca",
    "3714428559952ccfe814827221afac3b2c28d36b970115a8af8461462d34cee092df76b5f2c8dc976d14cb57296853b0a4b9d46e27ad977260"
    "22f366474eb8ee",
    "e1e415af2adc16718fd9b8a2ec5d5b6915755a1fbead53209c5e93ad722fe15c67c15e9d4e9696d513cb3fbfc24d1e213697eac30797e900ef"
    "c2e466c461c5ad",
    "ac7a708b01a03e196a18a8dc5c44e01a5996ee1f4485e221787c3c7cafdceb468e55bf6ca43c1a4f765370e6d2e3fdcfa19e4ba03db34fbcd2"
    "088b435917d4ee",
    "36ef380811dd1afd0e6362aaf879a9fd7507fe4a17e9b6eb6e023974dcf375b01a1c9b955e14a856ca7c1d8110711d121c8261622fe0267ca2"
    "64a150a8fc37a3",
    "7d34c0ddafeb75566e7af3059ccaa86b57936389fd5e027b1bd4d9464a79a14c99dbe2e26ebcb44d434cc76dc178bad87483a1941f748003a4"
    "e41be392a9e256",
    "0abfbaf8690792fbbca88aaa2b005046993f7e737f031fbcce1d4289b94348bfd3305ee2f2e624bbe4e6e6d30ba66d6a42364dbbd18708be17"
    "f0e2148860cc1d",
    "b5b5aeeed701ba6ae24c3810d7ca556291306c6c571167d7f2003ec5b42e05751ca868d0312d4fc01cdf991b10fa4d7bfc52e3ad6135f61941"
    "44dbff1d73fb1f",
    "fce97386c1c561167ee38a22ba13d31872c32382f2b34c7d4fb34c546e6b5adcba7ddeb076047b64a22fc7220122ef958bad3e314f45044611"
    "e86967392a4640",
    "66c3e3fec4e675dcc483757c6d2fc4786027854c7b23ab481ea3dcad9241beb3d6cecac0bab4c7bbf4db9c2a8579eba715854cb3c28fcf8b94"
    "09f4c9e0ee90ae",
    "391629158a3f51c451b67b055b3cf696cf1ef98c6cc04101cb0374b19fc3dcba43f4853e5f11c1874e5289f94f623332eb5d5cb99e2f25cc07"
    "6be3c036213843",
    "669e46bcfa19d8035daa3f5ed91ad64af169517a1ffbdcb154f2d74e131844c3eb2ac5c3a8da3968e78a63b9ea3f54af25ba89c93cb8d7ef1e"
    "2af9ef983d7c1c",
    "dd6f2386ecd216b952c9bb83af587832f41a3a02d2818158566d283e8bf9afe802d673e27cb7f0ca4266a8024ca7cec2ed3ee5d2f4f07865eb"
    "c0549c282e7c40",
    "249fc2edf78939816129270d84f9f145097d698d17d01df77e02f549e2c52860757d2a38f9ec74b25dba29eedf7f95ec1daa9255673a60e317"
    "8f8fe68df05101",
    "49a0b504ba78fd28ebc6891766a924c1f0c3f90eae17df854a469eefb6a99b3838a77598aa99920d3249f0008c3e42175e5c1aacb61e7ec02b"
    "8b048a0907399e",
    "b6f26a189591ec5694c317b2d5c6ddca2bb8ae2b2379868ec4dc55733081412d3cb3ec0001d207bc07ce873c604f3249aaa386476935481c21"
    "f6d5110fe6a2ba",
    "7954b6c26809c5c5c6c9dc2dde3b533663d56f730b25683ae56ad5f9e8a983de156aaec6506dd69cf2927ae93268c27c40f71f7638670deedb"
    "fd8edba06a4d08",
    "82923e2853fe8ab02782f649f6449b686bd9bd0fa128171ebd98c87f12a29feb5faee3ea994f059b766986dc75a501c09a90fa1181a7fa5def"
    "c3dcf28d6774df",
    "d4d57762c8783f8ffee3f173ed0e6d73b900c3f42fbaaaa7c59b5b2e60af980a6d0ac65f8a430a83e010aa9e53b5db920e25a01003aa8ccf3a"
    "42c45e4e894938"};

TEST_F(CryptoTest, leader_selection) {
  std::unordered_map<uint64_t, vrf_sk_t> high_stake_nodes;
  std::unordered_map<uint64_t, vrf_sk_t> low_stake_nodes;
  std::unordered_map<uint64_t, uint64_t> block_produced;

  std::srand(std::time(nullptr));
  const uint64_t committee_size = 20;
  const uint64_t rounds = 100;
  const uint64_t low_stake_nodes_num = 50;
  const uint64_t high_stake_nodes_num = 5;
  const uint64_t high_stake_nodes_power = 201;  // simulate testnet 1 + 20 * 50 / 5
  const uint64_t low_stake_nodes_power = 10;
  const auto valid_sortition_players =
      high_stake_nodes_num * high_stake_nodes_power + low_stake_nodes_num * low_stake_nodes_power;

  for (uint64_t i = 0; i < high_stake_nodes_num; i++) {
    high_stake_nodes.emplace(i, taraxa::vrf_wrapper::vrf_sk_t(vrf_secret_keys[i]));
  }
  for (uint64_t i = 0; i < low_stake_nodes_num; i++) {
    low_stake_nodes.emplace(i + high_stake_nodes_num,
                            taraxa::vrf_wrapper::vrf_sk_t(vrf_secret_keys[i + high_stake_nodes_num]));
  }

  const auto selector = [&](auto& outputs, const auto& msg, const auto& nodes, auto node_power) {
    for (const auto& n : nodes) {
      VrfPbftSortition sortition(n.second, msg);
      HashableVrf hash(sortition.output_, n.second);
      if (auto stake = VrfPbftSortition::getBinominalDistribution(node_power, valid_sortition_players, committee_size,
                                                                  hash.getHash())) {
        hash.iter = 1;
        auto lowest = hash.getHash();
        for (uint64_t j = 2; j <= stake; j++) {
          hash.iter = j;
          auto tmp = hash.getHash();
          if (tmp < lowest) lowest = std::move(tmp);
        }
        outputs.emplace(n.first, lowest);
      }
    }
  };

  for (uint64_t i = 0; i < rounds; i++) {
    const VrfPbftMsg msg(propose_vote_type, i, 1);
    std::unordered_map<uint64_t, dev::h256> outputs;

    selector(outputs, msg, high_stake_nodes, high_stake_nodes_power);
    selector(outputs, msg, low_stake_nodes, low_stake_nodes_power);

    // every round there needs to be a leader
    EXPECT_TRUE(outputs.size());
    const auto leader = *std::min_element(outputs.begin(), outputs.end(),
                                          [](const auto& i, const auto& j) { return i.second < j.second; });
    block_produced[leader.first]++;
  }

  auto high_stake_nodes_blocks = 0;
  auto low_stake_nodes_blocks = 0;
  for (const auto& node : block_produced) {
    if (node.first < high_stake_nodes_num) {
      high_stake_nodes_blocks += node.second;
    } else {
      low_stake_nodes_blocks += node.second;
    }
  }
  EXPECT_EQ(high_stake_nodes_blocks, 65);
  EXPECT_EQ(low_stake_nodes_blocks, 35);

  const auto stake_ratio = high_stake_nodes_power * high_stake_nodes_num * 100 / valid_sortition_players;
  const auto blocks_ratio = high_stake_nodes_blocks * 100 / rounds;
  EXPECT_EQ(stake_ratio, 66);
  EXPECT_EQ(blocks_ratio, 65);
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
