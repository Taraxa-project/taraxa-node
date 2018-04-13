/*
Copyright 2018 Ilja Honkonen
*/

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define RAPIDJSON_HAS_STDSTRING 1

#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "ledger_storage.hpp"
#include "signatures.hpp"
#include "transactions.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

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
		if (verbose and not boost::filesystem::portable_directory_name(ledger_path_str)) {
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

	// create subdirectory for account data
	auto accounts_path = ledger_path;
	accounts_path /= "accounts";
	if (not boost::filesystem::exists(accounts_path)) {
		try {
			if (verbose) {
				std::cout << "Accounts directory doesn't exist, creating..." << std::endl;
			}
			boost::filesystem::create_directory(accounts_path);
		} catch (const std::exception& e) {
			std::cerr << "Couldn't create a directory for accounts: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	/*
	Read and verify input
	*/

	taraxa::Transaction<CryptoPP::BLAKE2s, std::string> transaction;
	try {
		transaction.load(std::cin, verbose);
	} catch (const std::exception& e) {
		std::cerr << "Couldn't load transaction from stdin: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	/*
	Add transaction to ledger data
	*/

	const auto hash_comment = "hash:" + transaction.hash_hex;

	// make sure previous transaction exists and doesn't already have a next one
	if (transaction.previous_hex != "0000000000000000000000000000000000000000000000000000000000000000") {
		const auto previous_path = taraxa::get_transaction_path(transaction.previous_hex, transactions_path);
		if (not boost::filesystem::exists(previous_path)) {
			std::cerr << "Previous transaction " << previous_path
				<< " doesn't exist." << std::endl;
			return EXIT_FAILURE;
		}

		std::ifstream previous_file_old(previous_path.c_str());
		std::string previous_str;
		while (previous_file_old.good()) {
			std::string temp;
			std::getline(previous_file_old, temp);
			if (temp.size() > 0) {
				previous_str += temp;
			}
		}
		previous_file_old.close();

		rapidjson::Document previous_data;
		previous_data.Parse(previous_str.c_str());
		if (previous_data.HasParseError()) {
			std::cerr << "Couldn't parse json data of previous transaction at character position "
				<< previous_data.GetErrorOffset() << ": "
				<< rapidjson::GetParseError_En(previous_data.GetParseError())
				<< std::endl;
			return EXIT_FAILURE;
		}

		if (previous_data.HasMember("next")) {
			// TODO: only error out if previous has different next transaction
			std::cerr << "Previous transaction already has a next transaction." << std::endl;
			return EXIT_FAILURE;
		}

		// add current one as next of previous to make seeking easier
		if (verbose) {
			std::cout << "Appending transaction hash to previous transaction at "
				<< previous_path << std::endl;
		}
		previous_data.AddMember("next", rapidjson::StringRef(transaction.hash_hex), previous_data.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		previous_data.Accept(writer);
		std::ofstream previous_file_new(previous_path.c_str());
		previous_file_new << buffer.GetString() << std::endl;
		previous_file_new.close();

	// check that a genesis transaction doesn't exist for this account
	} else {
		const auto
			genesis_path = taraxa::get_account_path(transaction.pubkey_hex, accounts_path),
			genesis_dir = genesis_path.parent_path();
		if (boost::filesystem::exists(genesis_path)) {
			// TODO: only error out if different genesis provided
			std::cerr << "Genesis transaction already exists for account "
				<< transaction.pubkey_hex << std::endl;
			return EXIT_FAILURE;
		}
		if (not boost::filesystem::exists(genesis_dir)) {
			if (verbose) {
				std::cout << "Account directory doesn't exist, creating..." << std::endl;
			}
			boost::filesystem::create_directories(genesis_dir);
		}

		if (verbose) {
			std::cout << "Recording genesis to " << genesis_path << std::endl;
		}
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		rapidjson::Document account_data;
		account_data.SetObject();
		account_data.AddMember("genesis", rapidjson::StringRef(transaction.hash_hex), account_data.GetAllocator());
		account_data.Accept(writer);
		std::ofstream account_file(genesis_path.c_str());
		account_file << buffer.GetString() << std::endl;
	}

	/*
	Add transaction given on stdin to ledger data
	*/

	const auto
		transaction_path = taraxa::get_transaction_path(transaction.hash_hex, transactions_path),
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

	transaction.to_json_file(transaction_path.string());

	return EXIT_SUCCESS;
}
