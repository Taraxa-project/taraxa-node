/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"
#include "signatures.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	std::string exp_hex, receiver_hex, new_balance_str;

	boost::program_options::options_description options(
		"Sign a hex encoded message without the leading 0x given on "
		"standard input using a key produced with generate_private_key, "
		"for example. Prints the hex encoded result to standard output.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("receiver", boost::program_options::value<std::string>(&receiver_hex),
			"Receiver of the transaction, hex encoded without leading 0x")
		("new-balance", boost::program_options::value<std::string>(&new_balance_str),
			"Balance after the transfer, given as (decimal) number")
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

	if (receiver_hex.size() != 128) {
		std::cerr << "Receiver address must be 128 characters but is " << receiver_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	const std::string
		exp_bin = taraxa::hex2bin(exp_hex),
		receiver_bin = taraxa::hex2bin(receiver_hex);

	CryptoPP::Integer new_balance(new_balance_str.c_str());
	std::string new_balance_bin(8, 0);
	new_balance.Encode(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_bin.data())), 8);

	const auto signature = taraxa::sign_message(new_balance_bin + receiver_bin, exp_bin);
	std::cout << taraxa::bin2hex(signature) << " "
		<< taraxa::bin2hex(new_balance_bin) << " "
		<< taraxa::bin2hex(receiver_bin) << std::endl;

	return EXIT_SUCCESS;
}
