/*
Copyright 2018 Ilja Honkonen
*/

#ifndef SIGNATURES_HPP
#define SIGNATURES_HPP


#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <string>


namespace taraxa {


/*
@exponent must be in binary form.

Usually @Chars = std::string.
*/
template<class Chars> Chars sign_message(
	const Chars& message,
	const Chars& exponent
) {
	CryptoPP::Integer exp;
	exp.Decode(reinterpret_cast<const CryptoPP::byte*>(exponent.data()), exponent.size());

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	private_key.Initialize(CryptoPP::ASN1::secp256r1(), exp);

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Signer signer(private_key);
	Chars signature(signer.MaxSignatureLength(), 0);

	CryptoPP::AutoSeededRandomPool prng;
	const auto signature_length = signer.SignMessage(
		prng,
		reinterpret_cast<const CryptoPP::byte*>(message.data()),
		message.size(),
		reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(signature.data()))
	);
	signature.resize(signature_length);

	return signature;
}


/*
Verifies @signature of @message.

Uses x and y coordinates of public point.
All parameters must be given in binary form.

Usually @Chars = std::string.
*/
template<class Chars> bool verify_signature(
	const Chars& signature,
	const Chars& message,
	const Chars& x_bin,
	const Chars& y_bin
) {

	CryptoPP::Integer x, y;
	x.Decode(reinterpret_cast<const CryptoPP::byte*>(x_bin.data()), x_bin.size());
	y.Decode(reinterpret_cast<const CryptoPP::byte*>(y_bin.data()), y_bin.size());

	const CryptoPP::ECP::Point point{x, y};
	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
	public_key.Initialize(CryptoPP::ASN1::secp256r1(), point);

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Verifier verifier(public_key);
	return verifier.VerifyMessage(
		reinterpret_cast<const CryptoPP::byte*>(message.data()), message.size(),
		reinterpret_cast<const CryptoPP::byte*>(signature.data()), signature.size()
	);
}

} // namespace taraxa

#endif // ifndef SIGNATURES_HPP
