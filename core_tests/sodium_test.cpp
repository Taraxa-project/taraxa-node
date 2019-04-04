/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2018-04-02
 * @Last Modified by:
 * @Last Modified time:
 */

#include "taraxa_sodium.h"
#include "libdevcore/Log.h"

#include <gtest/gtest.h>

namespace taraxa {

string pk_output("3B6A27BCCEB6A42D62A3A8D02A6F0D73653215771DE243A63AC048A18B59DA29");
string sk_output("00000000000000000000000000000000000000000000000000000000000000003B6A27BCCEB6A42D62A3A8D02A6F0D73653215771DE243A63AC048A18B59DA29");
string proof_output("F66C1CC487BAB955F30682A3C13209143B1AFD1D7A273460697D7DA3F701C2441BB7FA37A711084AF48E0B45B915E86D643DBB4D9BAC85A174F39052837955BC8E62353A35091D34BC0B42AB3BD6DA0C");
string proof_hash_output("1AC48EAD1DE818E341956F07EDF2377C3AF13E944B254DA81E0B6693C25E378C414FDEDF7907B51C5AF5C39A5703DBA481B227B920350730DF660D75ADA5C789");

TEST(TaraxaSodium, sodium_message_test) {
  string seed_hex(64, '0');
  string message_hex("taraxa");
  KeyPair key_pair;
  TaraxaSodium sodium;

  key_pair = sodium.generate_key_pair_from_seed(seed_hex);
  std::cout << "public key: " << key_pair.public_key_hex << std::endl;
  std::cout << "secret key: " << key_pair.secret_key_hex << std::endl;
  EXPECT_EQ(key_pair.public_key_hex, pk_output);
  EXPECT_EQ(key_pair.secret_key_hex, sk_output);

  string proof_hex = sodium.get_vrf_proof(key_pair.secret_key_hex, message_hex);
  std::cout << "signature: " << proof_hex << std::endl;
  EXPECT_EQ(proof_hex, proof_output);

  string proof_hash_hex = sodium.get_vrf_proof_to_hash(proof_hex);
  std::cout << "proof hash: " << proof_hash_hex << std::endl;
  EXPECT_EQ(proof_hash_hex, proof_hash_hex);

  bool verify = sodium.verify_vrf_proof(key_pair.public_key_hex, proof_hex, message_hex);
  EXPECT_EQ(verify, true);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}