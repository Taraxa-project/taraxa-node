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

	std::string data;

	boost::program_options::options_description options(
		"Generates a key and prints it's private exponent to standard output, "
		"which can be given e.g. to print_key_info.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit");

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

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	CryptoPP::AutoSeededRandomPool prng;
	private_key.Initialize(prng, CryptoPP::ASN1::secp160r1());
	const CryptoPP::Integer& exponent = private_key.GetPrivateExponent();

	std::string exp_bin;
	exp_bin.resize(20); // TODO: use named constant
	exponent.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(exp_bin.data())), exp_bin.size());
	std::cout << taraxa::bin2hex(exp_bin) << std::endl;

	return EXIT_SUCCESS;
}
