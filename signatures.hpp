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


/*!
Returns private exponent and public X and Y coordinates derived from it.

Arguments must be, and returned values are, hex encoded without the leading 0x.

Usually @Chars = std::string.
*/
template<class Chars> std::array<Chars, 3> get_public_key_hex(const Chars& private_exponent) {
	using std::to_string;

	if (private_exponent.size() != 64) {
		throw std::invalid_argument(
			"Private exponent must be 64 chars but is "
			+ to_string(private_exponent.size()) + " chars."
		);
	}
	const std::string exp_bin = taraxa::hex2bin(private_exponent);

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
	const auto public_element = public_key.GetPublicElement();
	const auto
		&x_pub = public_element.x,
		&y_pub = public_element.y;

	std::string x_bin, y_bin;
	x_bin.resize(32); // TODO use named constant
	y_bin.resize(32);

	x_pub.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(x_bin.data())), x_bin.size());
	y_pub.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(y_bin.data())), y_bin.size());

	// return private exponent that was used, padded to 64 characters
	auto exp_hex = taraxa::bin2hex(exp_bin);
	if (exp_hex.size() < 64) {
		exp_hex.insert(0, 64 - exp_hex.size(), '0');
	}
	if (exp_hex.size() > 64) {
		throw std::logic_error(
			"Private exponent is " + to_string(exp_hex.size()) + " chars."
		);
	}
	return {exp_hex, taraxa::bin2hex(x_bin), taraxa::bin2hex(y_bin)};
}


/*!
@exponent must be in binary form.

Usually @Chars = std::string.
*/
template<class Chars> Chars sign_message_bin(
	const Chars& message,
	const Chars& exponent
) {
	// TODO: add error checking

	CryptoPP::Integer exp;
	exp.Decode(reinterpret_cast<const CryptoPP::byte*>(exponent.data()), exponent.size());

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	private_key.Initialize(CryptoPP::ASN1::secp256r1(), exp);
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


//! As sign_message_bin but first converts inputs from hex to binary
template<class Chars> Chars sign_message_hex(
	const Chars& message_hex,
	const Chars& exponent_hex
) {
	return sign_message_bin(
		hex2bin(message_hex),
		hex2bin(exponent_hex)
	);
}


/*!
Verifies @signature of @message.

Uses x and y coordinates of public point.
All parameters must be given in binary form.

Usually @Chars = std::string.
*/
template<class Chars> bool verify_signature_bin(
	const Chars& signature,
	const Chars& message,
	const Chars& x_bin,
	const Chars& y_bin
) {
	//TODO: add error checking

	CryptoPP::Integer x, y;
	x.Decode(reinterpret_cast<const CryptoPP::byte*>(x_bin.data()), x_bin.size());
	y.Decode(reinterpret_cast<const CryptoPP::byte*>(y_bin.data()), y_bin.size());

	const CryptoPP::ECP::Point point{x, y};
	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
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
}


//! As verify_signature_bin but first converts inputs from hex to binary
template<class Chars> bool verify_signature_hex(
	const Chars& signature,
	const Chars& message,
	const Chars& x_hex,
	const Chars& y_hex
) {
	if (x_hex.size() != 64) {
		throw std::invalid_argument(
			"X coordinate of public point must be 64 characters but is "
			+ std::to_string(x_hex.size())
		);
	}

	if (y_hex.size() != 64) {
		throw std::invalid_argument(
			"Y coordinate of public point must be 64 characters but is "
			+ std::to_string(y_hex.size())
		);
	}

	return verify_signature_bin(
		hex2bin(signature),
		hex2bin(message),
		hex2bin(x_hex),
		hex2bin(y_hex)
	);
}

} // namespace taraxa

#endif // ifndef SIGNATURES_HPP
