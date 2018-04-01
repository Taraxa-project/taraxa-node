/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"
#include "signatures.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	std::string exp_hex, receiver_hex, new_balance_str, payload_hex, previous_hex;

	boost::program_options::options_description options(
		"Creates an outgoing transaction ready to be sent over the network.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("previous", boost::program_options::value<std::string>(&previous_hex),
			"Hash of previous transaction from the account (hex)")
		("receiver", boost::program_options::value<std::string>(&receiver_hex),
			"Receiver of the transaction that must create to corresponding receive (hex)")
		("payload", boost::program_options::value<std::string>(&payload_hex),
			"Payload of the transaction (hex)")
		("new-balance", boost::program_options::value<std::string>(&new_balance_str),
			"Balance of sending account after the transaction, given as (decimal) "
			"number. Fee paid for the transaction is equal to balance before "
			"the transaction - balance after the transaction")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private exponent of the key used to sign the transaction (hex)");

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

	if (previous_hex.size() != 2 * CryptoPP::BLAKE2s::DIGESTSIZE) {
		std::cerr << "Hash of previous transaction must be " << 2 * CryptoPP::BLAKE2s::DIGESTSIZE
			<< " characters but is " << previous_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	if (receiver_hex.size() != 128) {
		std::cerr << "Receiver address must be 128 characters but is " << receiver_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	const std::string
		exp_bin = taraxa::hex2bin(exp_hex),
		receiver_bin = taraxa::hex2bin(receiver_hex),
		payload_bin = taraxa::hex2bin(payload_hex),
		previous_bin = taraxa::hex2bin(previous_hex);

	CryptoPP::Integer new_balance(new_balance_str.c_str());
	std::string new_balance_bin(8, 0);
	new_balance.Encode(
		reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_bin.data())),
		8
	);

	const auto signature = taraxa::sign_message(
		previous_bin + new_balance_bin + receiver_bin + payload_bin,
	exp_bin);

	std::cout << taraxa::bin2hex(signature) << " "
		<< taraxa::bin2hex(previous_bin) << " "
		<< taraxa::bin2hex(new_balance_bin) << " "
		<< taraxa::bin2hex(receiver_bin) << " "
		<< taraxa::bin2hex(payload_bin) << std::endl;

	return EXIT_SUCCESS;
}
