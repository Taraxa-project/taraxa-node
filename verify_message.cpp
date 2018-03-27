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

	std::string x_hex, y_hex, message;

	boost::program_options::options_description options(
		"Verify a signature given on standard input.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("x", boost::program_options::value<std::string>(&x_hex),
			"Verify using arg as the x coordinate of public point "
			"given as hex encoded string without the leading 0x")
		("y", boost::program_options::value<std::string>(&y_hex),
			"Verify using arg as the y coordinate of public point "
			"given as hex encoded string without the leading 0x")
		("message", boost::program_options::value<std::string>(&message),
			"Verify arg");

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

	if (x_hex.size() != 64) {
		std::cerr << "X coordinate of public point must be 64 characters but is "
			<< x_hex.size() << std::endl;
		return EXIT_FAILURE;
	}
	if (y_hex.size() != 64) {
		std::cerr << "Y coordinate of public point must be 64 characters but is "
			<< y_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	std::string signature_hex;
	std::cin >> signature_hex;

	const std::string
		x_bin = taraxa::hex2bin(x_hex),
		y_bin = taraxa::hex2bin(y_hex),
		signature_bin = taraxa::hex2bin(signature_hex);

	CryptoPP::Integer x, y;
	x.Decode(reinterpret_cast<const CryptoPP::byte*>(x_bin.data()), x_bin.size());
	y.Decode(reinterpret_cast<const CryptoPP::byte*>(y_bin.data()), y_bin.size());

	const CryptoPP::ECP::Point point{x, y};
	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
	public_key.Initialize(CryptoPP::ASN1::secp256r1(), point);

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Verifier verifier(public_key);
	const bool verified = verifier.VerifyMessage(
		reinterpret_cast<const CryptoPP::byte*>(message.data()), message.size(),
		reinterpret_cast<const CryptoPP::byte*>(signature_bin.data()), signature_bin.size()
	);

	if (verified) {
		if (option_variables.count("verbose") > 0) {
			std::cout << "PASS" << std::endl;
		}
		return EXIT_SUCCESS;
	} else {
		if (option_variables.count("verbose") > 0) {
			std::cout << "FAIL" << std::endl;
		}
		return EXIT_FAILURE;
	}
}
