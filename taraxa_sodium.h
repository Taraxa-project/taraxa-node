/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2018-04-02
 * @Last Modified by:
 * @Last Modified time:
 */

#ifndef TARAXA_SODIUM_H
#define TARAXA_SODIUM_H

#include <string>

namespace taraxa {
using std::string;

class KeyPair {
 public:
  string public_key_hex;
  string secret_key_hex;
  KeyPair() {}
  KeyPair(string pk_hex, string sk_hex):
    public_key_hex(pk_hex), secret_key_hex(sk_hex) {}
};

// Taraxa Sodium definition
class TaraxaSodium {
 public:
  TaraxaSodium() = default;
  ~TaraxaSodium() {};

  KeyPair generate_key_pair_from_seed(string seed_hex);

  string get_vrf_proof(string sk_hex, string message_hex);

  string get_vrf_proof_to_hash(string proof_hex);

  bool verify_vrf_proof(string pk_hex, string proof_hex, string message_hex);
};

} // namespace taraxa

#endif //TARAXA_SODIUM_H