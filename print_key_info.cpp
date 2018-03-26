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
		"Prints information about a key read from standard input.\n"
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

	std::string exp_hex;
	std::cin >> exp_hex;
	std::cout << "Private key:                " << exp_hex << std::endl;

	const std::string exp_bin = taraxa::hex2bin(exp_hex);
	CryptoPP::Integer exponent;
	exponent.Decode(reinterpret_cast<const CryptoPP::byte*>(exp_bin.data()), exp_bin.size());

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
	private_key.Initialize(CryptoPP::ASN1::secp160r1(), exponent);

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
	private_key.MakePublicKey(public_key);
	const auto public_element = public_key.GetPublicElement();
	const auto
		&x_pub = public_element.x,
		&y_pub = public_element.y;

	std::string x_bin, y_bin;
	x_bin.resize(20);
	y_bin.resize(20);

	x_pub.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(x_bin.data())), x_bin.size());
	y_pub.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(y_bin.data())), y_bin.size());

	std::cout << "Public key, first element:  " << taraxa::bin2hex(x_bin) << std::endl;
	std::cout << "Public key, second element: " << taraxa::bin2hex(y_bin) << std::endl;

	return EXIT_SUCCESS;
}
