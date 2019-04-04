/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2018-04-02
 * @Last Modified by:
 * @Last Modified time:
 */

#include "taraxa_sodium.h"
#include "bin2hex2bin.hpp"

#include <sodium.h>

#include <iostream>

namespace taraxa {

using std::cerr;
using std::endl;

KeyPair TaraxaSodium::generate_key_pair_from_seed(string seed_hex) {
  KeyPair key_pair;

  if (seed_hex.size() != 2 * crypto_vrf_SEEDBYTES) {
    cerr << "Seed must be "
         << 2 * crypto_vrf_SEEDBYTES
         << " characters but "
         << seed_hex.size()
         << " given"
         << endl;

    return key_pair;
  }

  const string seed_bin = taraxa::hex2bin(seed_hex);
  if (seed_bin.size() != crypto_vrf_SEEDBYTES) {
    cerr << "Seed must be "
         << crypto_vrf_SEEDBYTES
         << " bytes but "
         << seed_bin.size()
         << " given"
         << endl;

    return key_pair;
  }

  string public_key_bin(crypto_vrf_PUBLICKEYBYTES, ' ');
  string secret_key_bin(crypto_vrf_SECRETKEYBYTES, ' ');

  crypto_vrf_keypair_from_seed(
      (unsigned char *) public_key_bin.data(),
      (unsigned char *) secret_key_bin.data(),
      (unsigned char *) seed_bin.data());

  string pk_hex = taraxa::bin2hex(public_key_bin);
  string sk_hex = taraxa::bin2hex(secret_key_bin);
  key_pair.public_key_hex = pk_hex;
  key_pair.secret_key_hex = sk_hex;

  return key_pair;
}

string TaraxaSodium::get_vrf_proof(string sk_hex, string message_hex) {
  string proof_hex;

  if (sk_hex.size() != 2 * crypto_vrf_SECRETKEYBYTES) {
    cerr << "Secret key must be "
         << 2 * crypto_vrf_SECRETKEYBYTES
         << " characters but "
         << sk_hex.size()
         << " given"
         << endl;

    return proof_hex;
  }

  const string sk_bin = taraxa::hex2bin(sk_hex);
  if (sk_bin.size() != crypto_vrf_SECRETKEYBYTES) {
    cerr << "Secret key must be "
         << crypto_vrf_SECRETKEYBYTES
         << " bytes but "
         << sk_bin.size()
         << " given"
         << endl;

    return proof_hex;
  }

  const string message_bin = taraxa::hex2bin(message_hex);

  string proof_bin(crypto_vrf_PROOFBYTES, ' ');
  if (crypto_vrf_prove((unsigned char*) proof_bin.data(),
                       (unsigned char*) sk_bin.data(),
                       (unsigned char*) message_bin.data(),
                       message_bin.size()) != 0) {
    cerr << "Couldn't get proof." << endl;

    return proof_hex;
  }

  proof_hex = taraxa::bin2hex(proof_bin);

  return proof_hex;
}

string TaraxaSodium::get_vrf_proof_to_hash(string proof_hex) {
  string proof_hash_hex;

  if (proof_hex.size() != 2 * crypto_vrf_PROOFBYTES) {
    cerr << "Proof must be "
         << 2 * crypto_vrf_PROOFBYTES
         << " characters but "
         << proof_hex.size()
         << " given"
         << endl;

    return proof_hash_hex;
  }

  const string proof_bin = taraxa::hex2bin(proof_hex);
  if (proof_bin.size() != crypto_vrf_PROOFBYTES) {
    cerr << "Proof must be "
         << crypto_vrf_PROOFBYTES
         << " bytes but "
         << proof_bin.size()
         << " given"
         << endl;

    return proof_hash_hex;
  }

  string proof_hash_bin(crypto_vrf_OUTPUTBYTES, ' ');
  if (crypto_vrf_proof_to_hash((unsigned char*) proof_hash_bin.data(),
                               (unsigned char*) proof_bin.data()) != 0) {
    cerr << "Couldn't get hash of proof." << endl;

    return proof_hash_hex;
  }

  proof_hash_hex = taraxa::bin2hex(proof_hash_bin);

  return proof_hash_hex;
}

bool TaraxaSodium::verify_vrf_proof(string pk_hex,
                                    string proof_hex,
                                    string message_hex) {
  if (pk_hex.size() != 2 * crypto_vrf_PUBLICKEYBYTES) {
    cerr << "Public key must be "
         << 2 * crypto_vrf_PUBLICKEYBYTES
         << " characters but "
         << pk_hex.size()
         << " given"
         << endl;

    return false;
  }

  const string pk_bin = taraxa::hex2bin(pk_hex);
  if (pk_bin.size() != crypto_vrf_PUBLICKEYBYTES) {
    cerr << "Public key must be "
         << crypto_vrf_PUBLICKEYBYTES
         << " bytes but "
         << pk_bin.size()
         << " given"
         << endl;

    return false;
  }

  if (proof_hex.size() != 2 * crypto_vrf_PROOFBYTES) {
    cerr << "Proof must be "
         << 2 * crypto_vrf_PROOFBYTES
         << " characters but "
         << proof_hex.size()
         << " given"
         << endl;

    return false;
  }

  const string proof_bin = taraxa::hex2bin(proof_hex);
  if (proof_bin.size() != crypto_vrf_PROOFBYTES) {
    cerr << "Proof must be "
         << crypto_vrf_PROOFBYTES
         << " bytes but "
         << proof_bin.size()
         << " given"
         << endl;

    return false;
  }

  const string message_bin = taraxa::hex2bin(message_hex);

  string verify_bin(crypto_vrf_OUTPUTBYTES, ' ');
  if (crypto_vrf_verify((unsigned char*) verify_bin.data(),
                        (unsigned char*) pk_bin.data(),
                        (unsigned char*) proof_bin.data(),
                        (unsigned char*) message_bin.data(),
                        message_bin.size()) != 0) {
    cerr << "Verify proof fail" << endl;

    return false;
  }

  return true;
}

} // namespace taraxa