/*
Copyright 2018 Ilja Honkonen
*/

#include "bin2hex2bin.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace taraxa{
	std::string generate_private_key(){
	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	CryptoPP::AutoSeededRandomPool prng;
	private_key.Initialize(prng, CryptoPP::ASN1::secp256r1());
	if (not private_key.Validate(prng, 3)) {
		std::cerr << "Validation of private key failed!" << std::endl;
		return "";
	}
	const CryptoPP::Integer& exponent = private_key.GetPrivateExponent();

	std::string exp_bin;
	exp_bin.resize(32); // TODO: use named constant
	exponent.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(exp_bin.data())), exp_bin.size());
	return taraxa::bin2hex(exp_bin);
}
}
