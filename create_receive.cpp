/*
Copyright 2018 Ilja Honkonen
*/

#define RAPIDJSON_HAS_STDSTRING 1

#include "bin2hex2bin.hpp"
#include "signatures.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	/*
	Parse command line options and validate input
	*/

	std::string exp_hex, send_hex, previous_hex;

	boost::program_options::options_description options(
		"Creates a receive transaction corresponding to a send, ready to be "
		"sent over the network.\nAll hex encoded strings must be given without "
		"the leading 0x.\nUsage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("previous", boost::program_options::value<std::string>(&previous_hex),
			"Hash of previous transaction from the accepting account (hex)")
		("send", boost::program_options::value<std::string>(&send_hex),
			"Hash of send transaction that is accepted by the receive (hex)")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private exponent of the key used to sign the receive (hex)");

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

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		std::cerr << "Hash of previous transaction must be " << nr_hash_chars
			<< " characters but is " << previous_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	if (send_hex.size() != nr_hash_chars) {
		std::cerr << "Hash of send must be " << nr_hash_chars
			<< " characters but is " << send_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	/*
	Convert input to binary and sign
	*/

	auto
		exp_bin = taraxa::hex2bin(exp_hex),
		send_bin = taraxa::hex2bin(send_hex),
		previous_bin = taraxa::hex2bin(previous_hex);

	const auto
		signature_bin = taraxa::sign_message_bin(previous_bin + send_bin, exp_bin),
		signature_hex = taraxa::bin2hex(signature_bin);

	/*
	Convert send to JSON and print to stdout
	*/

	rapidjson::Document document;
	document.SetObject();

	// output what was signed
	send_hex = taraxa::bin2hex(send_bin);
	previous_hex = taraxa::bin2hex(previous_bin);

	auto& allocator = document.GetAllocator();
	document.AddMember("signature", rapidjson::StringRef(signature_hex), allocator);
	document.AddMember("previous", rapidjson::StringRef(previous_hex), allocator);
	document.AddMember("send", rapidjson::StringRef(send_hex), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	std::cout << buffer.GetString() << std::endl;

	return EXIT_SUCCESS;
}
