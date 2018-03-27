/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	std::string exp_hex;

	boost::program_options::options_description options(
		"Sign a message given on standard input using a key produced "
		"with generate_private_key, for example.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Sign using arg as the private exponent of the key "
			"given as hex encoded string without the leading 0x");

	boost::program_options::variables_map option_variables;
	boost::program_options::store(
		boost::program_options::parse_command_line(argc, argv, options),
		option_variables
	);
	boost::program_options::notify(option_variables);

	if (option_variables.count("help") > 0) {
		std::cout << options << std::endl;
		return EXIT_SUCCESS;
	}

	if (exp_hex.size() != 64) {
		std::cerr << "Key must be 64 characters but is " << exp_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	const std::string exp_bin = taraxa::hex2bin(exp_hex);
	CryptoPP::Integer exponent;
	exponent.Decode(reinterpret_cast<const CryptoPP::byte*>(exp_bin.data()), exp_bin.size());

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	private_key.Initialize(CryptoPP::ASN1::secp256r1(), exponent);
	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Signer signer(private_key);

	std::string message, signature(signer.MaxSignatureLength(), 0);
	std::cin >> message;

	CryptoPP::AutoSeededRandomPool prng;
	const auto signature_length = signer.SignMessage(
		prng,
		reinterpret_cast<const CryptoPP::byte*>(message.data()),
		message.size(),
		reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(signature.data()))
	);
	signature.resize(signature_length);

	std::cout << taraxa::bin2hex(signature);

	return EXIT_SUCCESS;
}
