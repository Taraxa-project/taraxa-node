/*
Copyright 2018 Ilja Honkonen
*/

#ifndef SIGNATURES_HPP
#define SIGNATURES_HPP

#include "bin2hex2bin.hpp"

#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <array>
#include <stdexcept>
#include <string>


namespace taraxa {


/*
Usually @Public_Key == CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey
*/
template<class Public_Key> size_t public_key_size(const Public_Key&) {
	Public_Key key;
	// segfault without Initialize before GetCurve and EncodedPointSize
	key.Initialize(CryptoPP::ASN1::secp256r1(), CryptoPP::ECP::Point());
	return key.GetGroupParameters().GetCurve().EncodedPointSize(true);
}


/*!
Returns private key first and public key second, derived from private key.

Input and output are hex encoded strings without the leading 0x.
Non hex characters in input are ignored.

Usually @Chars = std::string.
*/
template<class Chars> std::array<Chars, 2> get_public_key_hex(const Chars& private_key_hex) {
	using std::to_string;

	// TODO use named constant
	if (private_key_hex.size() != 64) {
		throw std::invalid_argument(
			"Hex encoded private key must be 64 chars but is "
			+ to_string(private_key_hex.size()) + " chars."
		);
	}
	const std::string exp_bin = hex2bin(private_key_hex);

	CryptoPP::Integer exponent;
	exponent.Decode(reinterpret_cast<const CryptoPP::byte*>(exp_bin.data()), exp_bin.size());

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	private_key.Initialize(CryptoPP::ASN1::secp256r1(), exponent);
	CryptoPP::AutoSeededRandomPool prng;
	if (not private_key.Validate(prng, 3)) {
		throw std::invalid_argument("Validation of private key failed!");
	}

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
	private_key.MakePublicKey(public_key);
	if (not public_key.Validate(prng, 3)) {
		throw std::invalid_argument("Validation of public key failed!");
	}

	const CryptoPP::ECP::Point point{
		public_key.GetPublicElement().x,
		public_key.GetPublicElement().y
	};

	const auto& curve = public_key.GetGroupParameters().GetCurve();
	std::string pubkey_bin(curve.EncodedPointSize(true), 0);
	curve.EncodePoint(
		reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(pubkey_bin.data())),
		point,
		true
	);

	// return private exponent that was used, padded to 64 characters
	auto exp_hex = bin2hex(exp_bin);
	if (exp_hex.size() < 64) {
		exp_hex.insert(0, 64 - exp_hex.size(), '0');
	}
	if (exp_hex.size() > 64) {
		throw std::logic_error(
			"Private exponent is " + to_string(exp_hex.size()) + " chars."
		);
	}
	return {exp_hex, bin2hex(pubkey_bin)};
}


/*!
Returns signature from signing @message with @private_key_bin.

Input and output is in binary form.

Usually @Chars = std::string.
*/
template<class Chars> Chars sign_message_bin(
	const Chars& message,
	const Chars& private_key_bin
) {
	if (private_key_bin.size() != 32) {
		throw std::invalid_argument(
			"Binary private key must be 32 bytes but is "
			+ std::to_string(private_key_bin.size()) + " bytes."
		);
	}

	CryptoPP::Integer exponent;
	exponent.Decode(
		reinterpret_cast<const CryptoPP::byte*>(private_key_bin.data()),
		private_key_bin.size()
	);

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	private_key.Initialize(CryptoPP::ASN1::secp256r1(), exponent);
	CryptoPP::AutoSeededRandomPool prng;
	if (not private_key.Validate(prng, 3)) {
		throw std::invalid_argument("Validation of private key failed!");
	}

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Signer signer(private_key);
	Chars signature(signer.MaxSignatureLength(), 0);

	const auto signature_length = signer.SignMessage(
		prng,
		reinterpret_cast<const CryptoPP::byte*>(message.data()),
		message.size(),
		reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(signature.data()))
	);
	signature.resize(signature_length);

	return signature;
}


//! As sign_message_bin but input and output are in hex format without leading 0x
template<class Chars> Chars sign_message_hex(
	const Chars& message_hex,
	const Chars& private_key_hex
) {
	if (private_key_hex.size() != 64) {
		throw std::invalid_argument(
			"Hex encoded private key must be 64 chars but is "
			+ std::to_string(private_key_hex.size()) + " chars."
		);
	}

	return bin2hex(sign_message_bin(hex2bin(message_hex), hex2bin(private_key_hex)));
}


/*!
Verifies @signature of @message agains public key @pubkey.

All parameters must be given in binary.

Usually @Chars = std::string.
*/
template<class Chars> bool verify_signature_bin(
	const Chars& signature,
	const Chars& message,
	const Chars& pubkey
) try {
	using std::to_string;

	const auto pubkey_size = public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (pubkey.size() != pubkey_size) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + "): "
			" Size of binary public key is " + to_string(pubkey.size())
			+ " but should be " + to_string(pubkey_size)
		);
	}

	/*
	Initialize public key from @pubkey
	*/

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
	// segfault without Initialize before GetCurve
	public_key.Initialize(CryptoPP::ASN1::secp256r1(), CryptoPP::ECP::Point());
	CryptoPP::ECP::Point point;
	if (not public_key.GetGroupParameters().GetCurve().DecodePoint(
		point,
		reinterpret_cast<const CryptoPP::byte*>(pubkey.data()),
		pubkey.size()
	)) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + "): "
			" Couldn't decode binary public key"
		);
	}
	public_key.Initialize(CryptoPP::ASN1::secp256r1(), point);

	CryptoPP::AutoSeededRandomPool prng;
	if (not public_key.Validate(prng, 3)) {
		throw std::invalid_argument("Validation of public key failed!");
	}

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Verifier verifier(public_key);
	return verifier.VerifyMessage(
		reinterpret_cast<const CryptoPP::byte*>(message.data()), message.size(),
		reinterpret_cast<const CryptoPP::byte*>(signature.data()), signature.size()
	);
} catch (...) {
	throw;
}


//! As verify_signature_bin but input and output is in hex without leading 0x
template<class Chars> bool verify_signature_hex(
	const Chars& signature,
	const Chars& message,
	const Chars& pubkey_hex
) {
	using std::to_string;

	const auto pubkey_hex_size = 2 * public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);

	if (pubkey_hex.size() != pubkey_hex_size) {
		throw std::invalid_argument(
			"Hex encoded public key must be " + to_string(pubkey_hex_size)
			+ " characters but is " + to_string(pubkey_hex.size())
		);
	}

	return verify_signature_bin(hex2bin(signature), hex2bin(message), hex2bin(pubkey_hex));
}

} // namespace taraxa

#endif // ifndef SIGNATURES_HPP
