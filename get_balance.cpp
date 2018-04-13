/*
Copyright 2018 Ilja Honkonen
*/

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define RAPIDJSON_HAS_STDSTRING 1

#include "accounts.hpp"
#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "ledger_storage.hpp"
#include "signatures.hpp"
#include "transactions.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <rapidjson/document.h>
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
	std::string ledger_path_str, transaction_str, account_str;

	boost::program_options::options_description options(
		"Prints the balance(s) from stored ledger.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"If not given either transaction or account, prints final "
		"balances of all accounts.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("verbose", "Print more information during execution")
		("ledger-path",
		 	boost::program_options::value<std::string>(&ledger_path_str),
			"Ledger data is located in directory arg")
		("transaction",
		 	boost::program_options::value<std::string>(&transaction_str),
			"Print balance of transaction's account after transaction arg")
		("account",
		 	boost::program_options::value<std::string>(&account_str),
			"Print final balance of account arg");

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

	if (transaction_str.size() > 0 and transaction_str.size() != 64) {
		std::cerr << "Transaction must be 64 characters but is "
			<< transaction_str.size() << std::endl;
		return EXIT_FAILURE;
	}

	if (account_str.size() > 0 and account_str.size() != 128) {
		std::cerr << "Account must be 128 characters but is "
			<< account_str.size() << std::endl;
		return EXIT_FAILURE;
	}

	/*
	Load ledger data
	*/
	boost::filesystem::path ledger_path(ledger_path_str);

	if (ledger_path.size() > 0) {
		if (not boost::filesystem::exists(ledger_path)) {
			std::cerr << "Ledger directory doesn't exist." << std::endl;
			return EXIT_FAILURE;
		}
		if (not boost::filesystem::is_directory(ledger_path)) {
			std::cerr << "Ledger path isn't a directory." << std::endl;
			return EXIT_FAILURE;
		}
	}

	// load account data
	auto accounts_path = ledger_path;
	accounts_path /= "accounts";
	if (not boost::filesystem::exists(accounts_path)) {
		std::cerr << "Accounts directory "
			<< accounts_path << " doesn't exist." << std::endl;
		return EXIT_FAILURE;
	}
	if (verbose) {
		std::cout << "Reading account data from "
			<< accounts_path << std::endl;
	}

	std::map<std::string, taraxa::Account> accounts; // key == hex address
	for (const auto& account_path: boost::filesystem::recursive_directory_iterator(accounts_path)) {
		if (boost::filesystem::is_directory(account_path)) {
			continue;
		}

		taraxa::Account account;
		account.pubkey_hex
			= account_path.path().parent_path().filename().string()
			+ account_path.path().filename().string();

		// get genesis transaction
		std::ifstream account_file(account_path.path().string());
		std::string json_str;
		while (account_file.good()) {
			std::string temp;
			std::getline(account_file, temp);
			if (temp.size() > 0) {
				json_str += temp;
			}
		}
		account_file.close();

		rapidjson::Document json;
		json.Parse(json_str.c_str());
		if (json.HasParseError()) {
			std::cerr << "Couldn't parse json data of account "
				<< account.pubkey_hex << " at character position "
				<< json.GetErrorOffset() << ": "
				<< rapidjson::GetParseError_En(json.GetParseError())
				<< std::endl;
			return EXIT_FAILURE;
		}

		if (not json.HasMember("genesis")) {
			std::cerr << "Account " << account.pubkey_hex
				<< " doesn't have a genesis transaction." << std::endl;
			return EXIT_FAILURE;
		}
		const auto& genesis_json = json["genesis"];
		if (not genesis_json.IsString()) {
			std::cerr << "Value of genesis transaction for account "
				<< account.pubkey_hex << " isn't a string." << std::endl;
			return EXIT_FAILURE;
		}

		account.genesis_transaction_hex = genesis_json.GetString();
		accounts[account.pubkey_hex] = account;
	}

	// load transaction data
	auto transactions_path = ledger_path;
	transactions_path /= "transactions";
	if (not boost::filesystem::exists(transactions_path)) {
		std::cerr << "Transactions directory "
			<< transactions_path << " doesn't exist." << std::endl;
		return EXIT_FAILURE;
	}
	if (verbose) {
		std::cout << "Reading transaction data from "
			<< transactions_path << std::endl;
	}

	std::map<
		std::string,
		taraxa::Transaction<CryptoPP::BLAKE2s, std::string>
	> transactions; // key == hex hash
	for (const auto& transaction_path: boost::filesystem::recursive_directory_iterator(transactions_path)) {
		if (boost::filesystem::is_directory(transaction_path)) {
			continue;
		}

		taraxa::Transaction<CryptoPP::BLAKE2s, std::string> transaction;
		transaction.load(transaction_path.path().string(), verbose);
		transactions[transaction.hash_hex] = transaction;
	}
	if (verbose) {
		std::cout << "Loaded " << transactions.size() << " transactions" << std::endl;
	}


	return EXIT_SUCCESS;
}
