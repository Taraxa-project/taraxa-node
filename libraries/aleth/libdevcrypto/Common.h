// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
/**
 * Ethereum-specific data structures & algorithms.
 */

#pragma once

#include <libdevcore/Address.h>
#include <libdevcore/Common.h>
#include <libdevcore/Exceptions.h>
#include <libdevcore/FixedHash.h>

#include <mutex>

namespace dev {

using Secret = SecureFixedHash<32>;

/// A public key: 64 bytes.
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
using Public = h512;

/// A public key in compressed format: 33 bytes.
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
using PublicCompressed = FixedHash<33>;

/// A signature: 65 bytes: r: [0, 32), s: [32, 64), v: 64.
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
using Signature = h520;

/// A compact signature: 65 bytes: r: [0, 32), vs: [32, 64).
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
/// https://eips.ethereum.org/EIPS/eip-2098
using CompactSignature = h512;

struct SignatureStruct {
  SignatureStruct() = default;
  SignatureStruct(Signature const& _s) { *(h520*)this = _s; }
  SignatureStruct(h256 const& _r, h256 const& _s, byte _v) : r(_r), s(_s), v(_v) {}
  SignatureStruct(const CompactSignature& _s) : SignatureStruct(Signature(_s)) {
    v = ::byte(u256(s) >> 255);
    s = u256(s) & ((u256(1) << 255) - 1);
  }
  operator Signature() const { return *reinterpret_cast<h520 const*>(this); }

  /// @returns true if r,s,v values are valid, otherwise false
  bool isValid() const noexcept;

  h256 r;
  h256 s;
  ::byte v = 0;
};

/// Convert signature to compact form.
/// https://eips.ethereum.org/EIPS/eip-2098
h512 toCompact(const Signature& _s);

struct CompactSignatureStruct {
  CompactSignatureStruct() = default;
  CompactSignatureStruct(const CompactSignature& _s) { *(h512*)this = _s; }
  CompactSignatureStruct(const Signature& _s) : CompactSignatureStruct(toCompact(_s)) {}
  CompactSignatureStruct(h256 const& _r, h256 const& _vs) : r(_r), vs(_vs) {}
  operator CompactSignature() const { return *reinterpret_cast<h512 const*>(this); }

  /// @returns true if r,s,v values are valid, otherwise false
  bool isValid() const noexcept;

  h256 r;
  h256 vs;
};

/// A vector of secrets.
using Secrets = std::vector<Secret>;

/// Convert a secret key into the public key equivalent.
Public toPublic(Secret const& _secret);

/// Convert a compressed public key into the uncompressed equivalent.
Public toPublic(PublicCompressed const& _publicCompressed);

/// Convert a secret key into the public key in compressed format.
PublicCompressed toPublicCompressed(Secret const& _secret);

/// Convert a public key to address.
Address toAddress(Public const& _public);

/// Convert a secret key into address of public key equivalent.
/// @returns 0 if it's not a valid secret key.
Address toAddress(Secret const& _secret);

/// Encrypts plain text using Public key.
void encrypt(Public const& _k, bytesConstRef _plain, bytes& o_cipher);

/// Decrypts cipher using Secret key.
bool decrypt(Secret const& _k, bytesConstRef _cipher, bytes& o_plaintext);

/// Encrypt payload using ECIES standard with AES128-CTR.
void encryptECIES(Public const& _k, bytesConstRef _plain, bytes& o_cipher);

/// Encrypt payload using ECIES standard with AES128-CTR.
/// @a _sharedMacData is shared authenticated data.
void encryptECIES(Public const& _k, bytesConstRef _sharedMacData, bytesConstRef _plain, bytes& o_cipher);

/// Decrypt payload using ECIES standard with AES128-CTR.
bool decryptECIES(Secret const& _k, bytesConstRef _cipher, bytes& o_plaintext);

/// Decrypt payload using ECIES standard with AES128-CTR.
/// @a _sharedMacData is shared authenticated data.
bool decryptECIES(Secret const& _k, bytesConstRef _sharedMacData, bytesConstRef _cipher, bytes& o_plaintext);

/// Encrypts payload with random IV/ctr using AES128-CTR.
std::pair<bytes, h128> encryptSymNoAuth(SecureFixedHash<16> const& _k, bytesConstRef _plain);

/// Encrypts payload with specified IV/ctr using AES128-CTR.
bytes encryptAES128CTR(bytesConstRef _k, h128 const& _iv, bytesConstRef _plain);

/// Decrypts payload with specified IV/ctr using AES128-CTR.
bytesSec decryptAES128CTR(bytesConstRef _k, h128 const& _iv, bytesConstRef _cipher);

/// Encrypts payload with specified IV/ctr using AES128-CTR.
inline bytes encryptSymNoAuth(SecureFixedHash<16> const& _k, h128 const& _iv, bytesConstRef _plain) {
  return encryptAES128CTR(_k.ref(), _iv, _plain);
}
inline bytes encryptSymNoAuth(SecureFixedHash<32> const& _k, h128 const& _iv, bytesConstRef _plain) {
  return encryptAES128CTR(_k.ref(), _iv, _plain);
}

/// Decrypts payload with specified IV/ctr using AES128-CTR.
inline bytesSec decryptSymNoAuth(SecureFixedHash<16> const& _k, h128 const& _iv, bytesConstRef _cipher) {
  return decryptAES128CTR(_k.ref(), _iv, _cipher);
}
inline bytesSec decryptSymNoAuth(SecureFixedHash<32> const& _k, h128 const& _iv, bytesConstRef _cipher) {
  return decryptAES128CTR(_k.ref(), _iv, _cipher);
}

/// Recovers Public key from signed message hash.
Public recover(Signature const& _sig, h256 const& _hash);

/// Returns siganture of message hash.
Signature sign(Secret const& _k, h256 const& _hash);

/// Verify signature.
bool verify(Public const& _k, Signature const& _s, h256 const& _hash);

// Verify signature with compressed public key
bool verify(PublicCompressed const& _key, h512 const& _signature, h256 const& _hash);

/// Simple class that represents a "key pair".
/// All of the data of the class can be regenerated from the secret key
/// (m_secret) alone. Actually stores a tuplet of secret, public and address
/// (the right 160-bits of the public).
class KeyPair {
 public:
  /// Normal constructor - populates object from the given secret key.
  /// If the secret key is invalid the constructor succeeds, but public key
  /// and address stay "null".
  KeyPair(Secret const& _sec);

  /// Create a new, randomly generated object.
  static KeyPair create();

  /// Create from an encrypted seed.
  static KeyPair fromEncryptedSeed(bytesConstRef _seed, std::string const& _password);

  Secret const& secret() const { return m_secret; }

  /// Retrieve the public key.
  Public const& pub() const { return m_public; }

  /// Retrieve the associated address of the public key.
  Address const& address() const { return m_address; }

  bool operator==(KeyPair const& _c) const { return m_public == _c.m_public; }
  bool operator!=(KeyPair const& _c) const { return m_public != _c.m_public; }

 private:
  Secret m_secret;
  Public m_public;
  Address m_address;
};

namespace crypto {

DEV_SIMPLE_EXCEPTION(InvalidState);

/// Key derivation
h256 kdf(Secret const& _priv, h256 const& _hash);

/**
 * @brief Generator for non-repeating nonce material.
 * The Nonce class should only be used when a non-repeating nonce
 * is required and, in its current form, not recommended for signatures.
 * This is primarily because the key-material for signatures is
 * encrypted on disk whereas the seed for Nonce is not.
 * Thus, Nonce's primary intended use at this time is for networking
 * where the key is also stored in plaintext.
 */
class Nonce {
 public:
  /// Returns the next nonce (might be read from a file).
  static Secret get() {
    static Nonce s;
    return s.next();
  }

 private:
  Nonce() = default;

  /// @returns the next nonce.
  Secret next();

  std::mutex x_value;
  Secret m_value;
};

namespace ecdh {

bool agree(Secret const& _s, Public const& _r, Secret& o_s) noexcept;

}

namespace ecies {

bytes kdf(Secret const& _z, bytes const& _s1, unsigned kdByteLen);

}
}  // namespace crypto
}  // namespace dev
