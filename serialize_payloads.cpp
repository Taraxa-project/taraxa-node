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
		"Prints the payload(s) of transactions stored in ledger.\n"
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

	auto [accounts, transactions, votes]
		= taraxa::load_ledger_data<CryptoPP::BLAKE2s>(ledger_path_str, verbose);
	(void)votes; // unused, silence compiler warning

	std::string serialized_payloads;

	std::map<std::string, taraxa::Transaction<CryptoPP::BLAKE2s>> processed_transactions;

	if (verbose) {
		std::cout << "Processing genesis transaction(s)" << std::endl;
	}
	for (const auto& item: transactions) {
		const auto& transaction = item.second;

		if (transaction.previous_hex != "0000000000000000000000000000000000000000000000000000000000000000") {
			continue;
		}

		// regular first transactions receive from another account
		if (transaction.receiver_hex != transaction.pubkey_hex) {
			continue;
		}

		if (accounts.count(transaction.pubkey_hex) == 0) {
			throw std::logic_error(__FILE__ "(" + std::to_string(__LINE__) + ")");
		}

		if (verbose) {
			std::cout << "Adding payload of " << transaction.hash_hex.substr(0, 5)
				<< "..." << transaction.hash_hex.substr(transaction.hash_hex.size() - 5) << std::endl;
		}
		serialized_payloads += taraxa::hex2bin(transaction.payload_hex);
		processed_transactions[transaction.hash_hex] = transaction;
	}

	/*
	Iterate through transactions to discover requested balance(s).
	*/

	std::set<std::string> transactions_to_process;
	for (const auto& item: transactions) {
		const auto& transaction = item.second;
		if (processed_transactions.count(transaction.hash_hex) == 0) {
			transactions_to_process.insert(transaction.hash_hex);
		}
	}

	while (transactions_to_process.size() > 0) {

		// find next one
		taraxa::Transaction<CryptoPP::BLAKE2s> transaction_to_process;
		try {
			transaction_to_process = [&](){
				std::set<std::string> transactions_to_remove;

				for (const auto& transaction_hex: transactions_to_process) {
					if (transactions.count(transaction_hex) == 0) {
						throw std::runtime_error(__FILE__ "(" + std::to_string(__LINE__) + ")");
					}

					const auto& transaction = transactions.at(transaction_hex);

					// must process previous first
					if (
						transaction.previous_hex != "0000000000000000000000000000000000000000000000000000000000000000"
						and processed_transactions.count(transaction.previous_hex) == 0
					) {
						if (verbose) {
							std::cout << "Skipping " << transaction.hash_hex
								<< " because previous not processed" << std::endl;
						}
						continue;
					}

					// receive
					if (transaction.send_hex.size() > 0) {

						// process send first
						if (processed_transactions.count(transaction.send_hex) == 0) {
							if (verbose) {
								std::cout << "Skipping " << transaction.hash_hex
									<< " because matching send not processed" << std::endl;
							}
							continue;
						}

						return transaction;

					// send
					} else {
						return transaction;
					}
				}

				for (const auto& remove: transactions_to_remove) {
					transactions_to_process.erase(remove);
				}

				throw std::runtime_error("Couldn't process any transaction");
			}();
		} catch (...) {
			if (verbose) {
				std::cout << "No transactions left to process" << std::endl;
			}
			break;
		}

		if (verbose) {
			std::cout << "Processing " << transaction_to_process.hash_hex.substr(0, 5)
				<< "..." << transaction_to_process.hash_hex.substr(transaction_to_process.hash_hex.size() - 5) << std::endl;
		}

		// receive
		if (transaction_to_process.send_hex.size() > 0) {

			if (processed_transactions.count(transaction_to_process.send_hex) == 0) {
				throw std::logic_error(
					__FILE__ "(" + std::to_string(__LINE__) + ") Send "
					+ transaction_to_process.send_hex + " not processed for receive"
				);
			}

			const auto& send = processed_transactions.at(transaction_to_process.send_hex);

			if (processed_transactions.count(send.previous_hex) == 0) {
				throw std::logic_error(
					__FILE__ "(" + std::to_string(__LINE__) + ") Prior transaction "
					+ send.previous_hex + " for send " + send.hash_hex + " not processed"
				);
			}

			if (accounts.count(transaction_to_process.pubkey_hex) == 0) {
				throw std::logic_error(
					__FILE__ "(" + std::to_string(__LINE__) + ") "
					+ "No account for receive transaction "
					+ transaction_to_process.hash_hex
				);
			}

		// send
		} else {

			if (accounts.count(transaction_to_process.pubkey_hex) == 0) {
				throw std::logic_error(
					__FILE__ "(" + std::to_string(__LINE__) + ") "
					+ "No account for send transaction "
					+ transaction_to_process.hash_hex
				);
			}

			if (verbose) {
				const auto& hash = transaction_to_process.hash_hex;
				std::cout << "Adding payload of " << hash.substr(0, 5)
					<< "..." << hash.substr(hash.size() - 5) << std::endl;
			}
			serialized_payloads += taraxa::hex2bin(transaction_to_process.payload_hex);
		}

		processed_transactions[transaction_to_process.hash_hex] = transaction_to_process;
		transactions_to_process.erase(transaction_to_process.hash_hex);
	}

	std::cout << serialized_payloads << std::endl;

	return EXIT_SUCCESS;
}
