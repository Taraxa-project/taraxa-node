/*
Copyright 2018 Ilja Honkonen
*/

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define RAPIDJSON_HAS_STDSTRING 1

#include "bin2hex2bin.hpp"
#include "hashes.hpp"
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
#include <fstream>
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
	// normalize hex representation of previous transaction
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

	// check for signature and normalize
	if (not document.HasMember("signature")) {
		std::cerr << "JSON object doesn't have a signature key." << std::endl;
		return EXIT_FAILURE;
	}
	const auto& signature_json = document["signature"];
	if (not signature_json.IsString()) {
		std::cerr << "Value of signature is not a string." << std::endl;
		return EXIT_FAILURE;
	}
	const auto signature_hex
		= taraxa::bin2hex(
			taraxa::hex2bin(
				std::string(signature_json.GetString())
			)
		);
	if (verbose) {
		std::cout << "Signature: " << signature_hex << std::endl;
	}

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
	const auto pubkey_hex
		= taraxa::bin2hex(
			taraxa::hex2bin(
				std::string(pubkey_json.GetString())
			)
		);
	if (pubkey_hex.size() != 128) {
		std::cerr << "Public key must be " << 128
			<< " characters but is " << pubkey_hex.size() << std::endl;
		return EXIT_FAILURE;
	}
	if (verbose) {
		std::cout << "Public key: " << pubkey_hex << std::endl;
	}

	/*
	Verify signature
	*/

	std::string signature_payload_hex;

	// send or receive?
	if (not document.HasMember("send") and not document.HasMember("receiver")) {
		std::cerr << "JSON object doesn't have send or receiver key." << std::endl;
		return EXIT_FAILURE;
	}
	if (document.HasMember("send") and document.HasMember("receiver")) {
		std::cerr << "JSON object has send and receiver key." << std::endl;
		return EXIT_FAILURE;
	}
	// receive
	if (document.HasMember("send")) {

		const auto& send_json = document["send"];
		if (not send_json.IsString()) {
			std::cerr << "Value of send is not string." << std::endl;
			return EXIT_FAILURE;
		}
		const auto send_hex
			= taraxa::bin2hex(
				taraxa::hex2bin(
					std::string(send_json.GetString())
				)
			);

		const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
		if (send_hex.size() != nr_hash_chars) {
			std::cerr << "Hash of send transaction must be " << nr_hash_chars
				<< " characters but is " << send_hex.size() << std::endl;
			return EXIT_FAILURE;
		}

		signature_payload_hex = previous_hex + send_hex;

	// send
	} else {

		const auto& receiver_json = document["receiver"];
		if (not receiver_json.IsString()) {
			std::cerr << "Value of receiver is not string." << std::endl;
		}
		const auto receiver_hex
			= taraxa::bin2hex(
				taraxa::hex2bin(
					std::string(receiver_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "Receiver: " << receiver_hex << std::endl;
		}

		if (not document.HasMember("new-balance")) {
			std::cerr << "JSON object doesn't have new-balance key." << std::endl;
			return EXIT_FAILURE;
		}
		const auto& new_balance_json = document["new-balance"];
		if (not new_balance_json.IsString()) {
			std::cerr << "Value of new-balance is not string." << std::endl;
			return EXIT_FAILURE;
		}
		const auto new_balance_hex
			= taraxa::bin2hex(
				taraxa::hex2bin(
					std::string(new_balance_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "New balance: " << new_balance_hex << std::endl;
		}

		if (not document.HasMember("payload")) {
			std::cerr << "JSON object doesn't have payload key." << std::endl;
			return EXIT_FAILURE;
		}
		const auto& payload_json = document["payload"];
		if (not payload_json.IsString()) {
			std::cerr << "Value of payload is not string." << std::endl;
			return EXIT_FAILURE;
		}
		const auto payload_hex
			= taraxa::bin2hex(
				taraxa::hex2bin(
					std::string(payload_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "Payload: " << payload_hex << std::endl;
		}

		signature_payload_hex = previous_hex + new_balance_hex + receiver_hex + payload_hex;
	}

	if (not taraxa::verify_signature_hex(
		signature_hex,
		signature_payload_hex,
		pubkey_hex.substr(0, pubkey_hex.size() / 2),
		pubkey_hex.substr(pubkey_hex.size() / 2, pubkey_hex.size() / 2)
	)) {
		std::cerr << "Signature verification failed" << std::endl;
		return EXIT_FAILURE;
	}
	if (verbose) {
		std::cout << "Signature OK" << std::endl;
	}

	/*
	Calcualte hash
	*/

	const auto
		hash_payload_hex = signature_hex + signature_payload_hex,
		hash_hex = taraxa::get_hash_hex<CryptoPP::BLAKE2s>(hash_payload_hex),
		hash_comment = "hash:" + hash_hex;

	/*
	TODO: append current hash to previous transaction for easier seeking
	*/

	// genesis transaction doesn't have a previous one
	if (previous_hex != "0000000000000000000000000000000000000000000000000000000000000000") {
		const auto previous_path = taraxa::get_transaction_path(previous_hex, transactions_path);
		if (not boost::filesystem::exists(previous_path)) {
			std::cerr << "Previous transaction " << previous_path
				<< " doesn't exist." << std::endl;
			return EXIT_FAILURE;
		}

		// TODO: check that a genesis transaction doesn't exist for this account
	}

	/*
	Add transaction given on stdin to ledger data
	*/

	const auto
		transaction_path = taraxa::get_transaction_path(hash_hex, transactions_path),
		transaction_dir = transaction_path.parent_path();

	if (boost::filesystem::exists(transaction_path)) {
		if (verbose) {
			std::cerr << "Transaction already exists." << std::endl;
		}
		return EXIT_SUCCESS;
	}
	if (not boost::filesystem::exists(transaction_dir)) {
		if (verbose) {
			std::cout << "Transaction directory doesn't exist, creating..." << std::endl;
		}
		boost::filesystem::create_directories(transaction_dir);
	}
	if (verbose) {
		std::cout << "Writing transaction to " << transaction_path << std::endl;
	}

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	std::ofstream transaction_file(transaction_path.c_str());
	transaction_file << buffer.GetString() << std::endl;

	return EXIT_SUCCESS;
}
