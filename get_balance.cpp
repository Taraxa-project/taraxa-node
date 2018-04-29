/*
Copyright 2018 Ilja Honkonen
*/

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define RAPIDJSON_HAS_STDSTRING 1

#include "accounts.hpp"
#include "balances.hpp"
#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "ledger_storage.hpp"
#include "signatures.hpp"
#include "transactions.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <cryptopp/integer.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
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
			"Print balance of transaction's account after transaction arg "
			"(hex), overrides --account")
		("account",
		 	boost::program_options::value<std::string>(&account_str),
			"Print final balance of account arg (hex) instead of all "
			"accounts, overridden by --transaction");

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

	const auto hash_hex_size = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (transaction_str.size() > 0 and transaction_str.size() != hash_hex_size) {
		std::cerr << "Hex hash of transaction must be " << hash_hex_size
			<< " characters but is " << transaction_str.size() << std::endl;
		return EXIT_FAILURE;
	}

	const auto pubkey_hex_size = 2 * taraxa::public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (account_str.size() > 0 and account_str.size() != pubkey_hex_size) {
		std::cerr << "Account must be " << pubkey_hex_size
			<< " characters but is " << account_str.size() << std::endl;
		return EXIT_FAILURE;
	}


	auto [accounts, transactions, votes]
		= taraxa::load_ledger_data<CryptoPP::BLAKE2s>(ledger_path_str, verbose);
	(void)votes; // unused, silence compiler warning

	taraxa::update_balances<CryptoPP::BLAKE2s>(
		accounts,
		transactions,
		verbose
	);

	if (account_str.size() == 0 and transaction_str.size() == 0) {
		std::cout << "Final balances:" << std::endl;
		for (const auto& item: accounts) {
			const auto& account = item.second;

			CryptoPP::Integer balance;
			const auto balance_bin = taraxa::hex2bin(account.balance_hex);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(balance_bin.data())),
				balance_bin.size()
			);

			std::cout << account.address_hex.substr(0, 5) << "..."
				<< account.address_hex.substr(account.address_hex.size() - 5)
				<< ": " << balance << std::endl;
		}

	} else {

		if (account_str.size() > 0) {
			if (accounts.count(account_str) == 0) {
				std::cerr << "Account " << account_str.substr(0, 5)
					<< "..." << account_str.substr(account_str.size() - 5)
					<< " not found" << std::endl;
				return EXIT_FAILURE;
			}
			const auto& account = accounts.at(account_str);

			CryptoPP::Integer balance;
			const auto balance_bin = taraxa::hex2bin(account.balance_hex);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(balance_bin.data())),
				balance_bin.size()
			);

			std::cout << "Final balance of " << account_str.substr(0, 5)
				<< "..." << account_str.substr(account_str.size() - 5)
				<< ": " << balance << std::endl;
		}

		if (transaction_str.size() > 0) {
			if (transactions.count(transaction_str) == 0) {
				std::cerr << "Transaction " << transaction_str.substr(0, 5)
					<< "..." << transaction_str.substr(transaction_str.size() - 5)
					<< " not found" << std::endl;
				return EXIT_FAILURE;
			}
			const auto& transaction = transactions.at(transaction_str);

			CryptoPP::Integer balance;
			const auto balance_bin = taraxa::hex2bin(transaction.new_balance_hex);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(balance_bin.data())),
				balance_bin.size()
			);

			account_str = transaction.pubkey_hex;
			const auto hash = transaction.hash_hex;
			std::cout << "Balance of " << account_str.substr(0, 5)
				<< "..." << account_str.substr(account_str.size() - 5)
				<< " at " << hash.substr(0, 5) << "..." << hash.substr(hash.size() - 5)
				<< ": " << balance << std::endl;
		}
	}
	return EXIT_SUCCESS;
}
