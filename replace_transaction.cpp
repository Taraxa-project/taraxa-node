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
		"Reads a transaction from standard input and adds it to the ledger,\n"
		"potentially replacing an existing transaction and subsequent transactions\n"
		"in that blockchain, if the replacement has more votes as weighted by balance.\n"
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

	// create subdirectory for vote data
	auto votes_path = ledger_path;
	votes_path /= "votes";
	if (not boost::filesystem::exists(votes_path)) {
		try {
			if (verbose) {
				std::cout << "Votes directory doesn't exist, creating..." << std::endl;
			}
			boost::filesystem::create_directory(votes_path);
		} catch (const std::exception& e) {
			std::cerr << "Couldn't create a directory for votes: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	// read and verify input
	taraxa::Transaction<CryptoPP::BLAKE2s> new_candidate;
	try {
		new_candidate.load(std::cin, verbose);
	} catch (const std::exception& e) {
		std::cerr << "Couldn't load candidate from stdin: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	if (new_candidate.previous_hex == "0000000000000000000000000000000000000000000000000000000000000000") {
		std::cerr << "Genesis transaction(s) cannot be replaced" << std::endl;
		return EXIT_FAILURE;
	}

	auto [accounts, transactions, votes]
		= taraxa::load_ledger_data<CryptoPP::BLAKE2s>(ledger_path_str, verbose);

	taraxa::update_balances<CryptoPP::BLAKE2s>(
		accounts,
		transactions,
		verbose
	);

	// find conflicting transaction
	taraxa::Transaction<CryptoPP::BLAKE2s> old_transaction;
	for (const auto& item: transactions) {
		const auto& old_candidate =  item.second;
		if (old_candidate.previous_hex != new_candidate.previous_hex) {
			continue;
		}

		const auto old_candidate_path = [&](){
			try {
				return taraxa::get_transaction_path(old_candidate.hash_hex, transactions_path);
			} catch (const std::exception& e) {
				throw std::invalid_argument(std::string("Couldn't get path for old candidate: ") + e.what());
			}
		}();

		if (old_candidate.hash_hex == new_candidate.hash_hex) {
			if (verbose) {
				std::cout << "Candidate already exists" << std::endl;
			}
			// update modification time to make test makefiles simpler
			boost::filesystem::last_write_time(old_candidate_path, std::time(nullptr));
			return EXIT_SUCCESS;
		}

		if (verbose) {
			std::cout << "Found conflicting transaction: " << old_candidate.hash_hex << std::endl;
		}

		// tally votes
		CryptoPP::Integer old_total(0l), new_total(0l);
		for (const auto& vote_i: votes) {
			const auto& vote = vote_i.second;

			if (accounts.count(vote.pubkey_hex) == 0) {
				std::cerr << "Account " << vote.pubkey_hex
					<< " doesn't exist for vote "<< vote.hash_hex << std::endl;
				return EXIT_FAILURE;
			}

			/*
			TODO: use better algorithm than last known balance of voter
			*/
			const auto balance_bin = taraxa::hex2bin(accounts.at(vote.pubkey_hex).balance_hex);
			CryptoPP::Integer balance;
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(
					balance_bin.data())),
				balance_bin.size()
			);

			if (vote.candidate_hex == old_candidate.hash_hex) {
				old_total += balance;
			} else if (vote.candidate_hex == new_candidate.hash_hex) {
				new_total += balance;
			}
		}

		if (verbose) {
			std::cout << "Voting results, old transaction: " << old_total
				<< ", new transaction: " << new_total << std::endl;
		}

		if (new_total <= old_total) {
			if (verbose) {
				std::cout << "Keeping old transaction" << std::endl;
			}
			boost::filesystem::last_write_time(old_candidate_path, std::time(nullptr));
			return EXIT_SUCCESS;
		}

		old_transaction = old_candidate;
		break;
	}

	// update next transaction in previous to new candidate with more votes
	const auto previous_path = [&](){
		try {
			return taraxa::get_transaction_path(new_candidate.previous_hex, transactions_path);
		} catch (const std::exception& e) {
			throw std::invalid_argument(std::string("Couldn't get path for previous transaction: ") + e.what());
		}
	}();

	if (not boost::filesystem::exists(previous_path)) {
		std::cerr << "Previous transaction " << previous_path
			<< " doesn't exist." << std::endl;
		return EXIT_FAILURE;
	}

	taraxa::Transaction<CryptoPP::BLAKE2s> previous_transaction;
	try {
		previous_transaction.load(previous_path.string(), verbose);
	} catch (const std::exception& e) {
		std::cerr << "Couldn't load previous transaction from "
			<< previous_path << ": " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	if (verbose) {
		std::cout << "Updating previous transaction at "
			<< previous_path << std::endl;
	}
	previous_transaction.next_hex = new_candidate.hash_hex;
	previous_transaction.to_json_file(previous_path.string());

	/*
	Add transaction given on stdin to ledger data
	*/

	const auto
		new_candidate_path = [&](){
			try {
				return taraxa::get_transaction_path(new_candidate.hash_hex, transactions_path);
			} catch (const std::exception& e) {
				throw std::invalid_argument(std::string("Couldn't get path for new candidate: ") + e.what());
			}
		}(),
		new_candidate_dir = new_candidate_path.parent_path();

	if (boost::filesystem::exists(new_candidate_path)) {
		std::cerr << "Internal error: new transaction already exists." << std::endl;
		return EXIT_FAILURE;
	}
	if (not boost::filesystem::exists(new_candidate_dir)) {
		if (verbose) {
			std::cout << "Transaction directory doesn't exist, creating..." << std::endl;
		}
		boost::filesystem::create_directories(new_candidate_dir);
	}
	if (verbose) {
		std::cout << "Writing new transaction to " << new_candidate_path << std::endl;
	}

	new_candidate.to_json_file(new_candidate_path.string());

	if (old_transaction.hash_hex.size() == 0) {
		return EXIT_SUCCESS;
	}

	/*
	TODO: Also remove transactions that depend on old one from ledger.
	*/
	const auto old_transaction_path = [&](){
		try {
			return taraxa::get_transaction_path(old_transaction.hash_hex, transactions_path);
		} catch (const std::exception& e) {
			throw std::invalid_argument(
				std::string("Couldn't get path for old transaction ")
				+ old_transaction.hash_hex + ": " + e.what()
			);
		}
	}();

	if (boost::filesystem::exists(old_transaction_path)) {
		if (verbose) {
			std::cout << "Removing transaction " << old_transaction.hash_hex.substr(0, 5)
				<< "..." << old_transaction.hash_hex.substr(old_transaction.hash_hex.size() - 5)
				<< " with fewer votes" << std::endl;
		}
		boost::filesystem::remove(old_transaction_path);
		const auto parent = old_transaction_path.parent_path();
		if (boost::filesystem::is_empty(parent)) {
			if (verbose) {
				std::cout << "Removing empty parent directory" << std::endl;
			}
			boost::filesystem::remove(parent);
			const auto gparent = parent.parent_path();
			if (boost::filesystem::is_empty(gparent)) {
				if (verbose) {
					std::cout << "Removing empty grandparent directory" << std::endl;
				}
				boost::filesystem::remove(gparent);
			}
		}
	}

	return EXIT_SUCCESS;
}
