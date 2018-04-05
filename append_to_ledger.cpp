/*
Copyright 2018 Ilja Honkonen
*/

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define RAPIDJSON_HAS_STDSTRING 1

#include "bin2hex2bin.hpp"
#include "ledger_storage.hpp"
#include "signatures.hpp"

#include <cryptopp/blake2.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/error/en.h"

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	/*
	Parse command line options and validate input
	*/

	bool verbose = false;
	std::string ledger_path_str;

	boost::program_options::options_description options(
		"Reads a transaction from standard input and appends it to the ledger.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("verbose", "Print more information during execution")
		("ledger-path",
		 	boost::program_options::value<std::string>(&ledger_path_str),
			"Ledger data is located in directory arg");

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

	if (option_variables.count("verbose") > 0) {
		verbose = true;
	}

	/*
	Prepare ledger directory
	*/

	boost::filesystem::path ledger_path(ledger_path_str);

	if (ledger_path.size() > 0) {
		if (not boost::filesystem::portable_directory_name(ledger_path_str)) {
			std::cout << "WARNING: ledger-path isn't portable." << std::endl;
		}

		if (not boost::filesystem::exists(ledger_path)) {
			if (verbose) {
				std::cout << "Ledger directory doesn't exist, creating..." << std::endl;
			}

			try {
				boost::filesystem::create_directories(ledger_path);
			} catch (const std::exception& e) {
				std::cerr << "Couldn't create ledger directory '" << ledger_path
					<< "': " << e.what() << std::endl;
				return EXIT_FAILURE;
			}
		}

		if (not boost::filesystem::is_directory(ledger_path)) {
			std::cerr << "Ledger path isn't a directory: " << ledger_path << std::endl;
			return EXIT_FAILURE;
		}
	}

	// create subdirectory for transaction data
	auto transactions_path = ledger_path;
	transactions_path /= "transactions";
	if (not boost::filesystem::exists(transactions_path)) {
		try {
			if (verbose) {
				std::cout << "Transactions directory doesn't exist, creating..." << std::endl;
			}
			boost::filesystem::create_directory(transactions_path);
		} catch (const std::exception& e) {
			std::cerr << "Couldn't create a directory for transcations: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	/*
	Read and verify input
	*/

	// parse json
	std::string transaction_json;
	while (std::cin.good()) {
		std::string temp;
		std::getline(std::cin, temp);
		if (temp.size() > 0) {
			transaction_json += temp;
		}
	}

	if (verbose) {
		std::cout << "Read " << transaction_json.size()
			<< " characters from stdin" << std::endl;
	}

	rapidjson::Document document;
	document.Parse(transaction_json.c_str());
	if (document.HasParseError()) {
		std::cerr << "Couldn't parse json data at character position "
			<< document.GetErrorOffset() << ": "
			<< rapidjson::GetParseError_En(document.GetParseError())
			<< std::endl;
		return EXIT_FAILURE;
	}

	if (not document.IsObject()) {
		std::cerr << "JSON data doesn't represent an object ( {...} )." << std::endl;
		return EXIT_FAILURE;
	}

	// check for previous transaction
	if (not document.HasMember("previous")) {
		std::cerr << "JSON object doesn't have a previous key." << std::endl;
		return EXIT_FAILURE;
	}

	const auto& previous_json = document["previous"];
	if (not previous_json.IsString()) {
		std::cerr << "Value of previous is not a string." << std::endl;
		return EXIT_FAILURE;
	}
	// normalize hex representation of previous
	const auto previous_hex
		= taraxa::bin2hex(
			taraxa::hex2bin(
				std::string(previous_json.GetString())
			)
		);
	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		std::cerr << "Hash of previous transaction must be " << nr_hash_chars 
			<< " characters but is " << previous_hex.size() << std::endl;
		return EXIT_FAILURE;
	}
	if (verbose) {
		std::cout << "Previous transaction: " << previous_hex << std::endl;
	}

	// check for signature
	if (not document.HasMember("signature")) {
		std::cerr << "JSON object doesn't have a signature key." << std::endl;
		return EXIT_FAILURE;
	}

	const auto& signature_json = document["signature"];
	if (not signature_json.IsString()) {
		std::cerr << "Value of signature is not a string." << std::endl;
		return EXIT_FAILURE;
	}
	const auto signature_hex = signature_json.GetString();

	// check for public key
	if (not document.HasMember("public-key")) {
		std::cerr << "JSON object doesn't have a public-key key." << std::endl;
		return EXIT_FAILURE;
	}

	const auto& pubkey_json = document["public-key"];
	if (not pubkey_json.IsString()) {
		std::cerr << "Value of public-key is not a string." << std::endl;
		return EXIT_FAILURE;
	}
	// normalize
	const auto pubkey_hex
		= taraxa::bin2hex(
			taraxa::hex2bin(
				std::string(pubkey_json.GetString())
			)
		);

	// send or receive?

	/*
	Find previous transaction in ledger data

	Assume existing transactions in ledger data are correct
	*/

	const auto previous_path = taraxa::get_transaction_path(previous_hex, transactions_path);
	if (not boost::filesystem::exists(previous_path)) {
		std::cerr << "Previous transaction " << previous_path
			<< " doesn't exist." << std::endl;
		return EXIT_FAILURE;
	}

	/*
	Check signature of transaction from stdin
	*/


/*
	if (not document.HasMember("")) {
		std::cerr << << std::endl;
		return EXIT_FAILURE;
	}
*/
	return EXIT_SUCCESS;
}
