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
#include <cryptopp/integer.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <type_traits>


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

	std::map<std::string, taraxa::Account<CryptoPP::BLAKE2s>> accounts; // key == hex address
	for (const auto& account_path: boost::filesystem::recursive_directory_iterator(accounts_path)) {
		if (boost::filesystem::is_directory(account_path)) {
			continue;
		}

		taraxa::Account<CryptoPP::BLAKE2s> account;
		try {
			account.load(account_path.path().string(), verbose);
		} catch (const std::exception& e) {
			std::cerr << "Couldn't load account data from "
				<< account_path.path().string() << ": " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
		accounts[account.address_hex] = account;
	}
	if (verbose) {
		std::cout << "Loaded " << accounts.size() << " accounts" << std::endl;
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
		taraxa::Transaction<CryptoPP::BLAKE2s>
	> transactions; // key == hex hash
	for (const auto& transaction_path: boost::filesystem::recursive_directory_iterator(transactions_path)) {
		if (boost::filesystem::is_directory(transaction_path)) {
			continue;
		}

		taraxa::Transaction<CryptoPP::BLAKE2s> transaction;
		transaction.load(transaction_path.path().string(), verbose);
		transactions[transaction.hash_hex] = transaction;
	}
	if (verbose) {
		std::cout << "Loaded " << transactions.size() << " transactions" << std::endl;
	}

	/*
	Process genesis transactions without corresponding sends
	*/

	std::string final_balance_hex(16, '0');

	std::map<
		std::string, // hex hash
		taraxa::Transaction<CryptoPP::BLAKE2s>
	> processed_transactions;

	taraxa::Transaction<CryptoPP::BLAKE2s> transaction_to_print;
	taraxa::Account<CryptoPP::BLAKE2s> account_to_print;

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
			std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
			return EXIT_FAILURE;
		}

		auto& account = accounts.at(transaction.pubkey_hex);
		if (verbose) {
			const auto balance_bin = taraxa::hex2bin(transaction.new_balance_hex);
			CryptoPP::Integer balance(0l);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(balance_bin.data())),
				balance_bin.size()
			);
			std::cout << "Setting initial balance of account "
				<< account.address_hex.substr(0, 5) << "..."
				<< account.address_hex.substr(account.address_hex.size() - 5)
				<< " to " << balance << std::endl;
		}
		account.balance_hex = transaction.new_balance_hex;

		if (account_str.size() > 0 and account_str == account.pubkey_hex) {
			account_to_print = account;
		}
		if (transaction_str.size() > 0 and transaction_str == transaction.hash_hex) {
			transaction_to_print = transaction;
		}

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

	std::set<std::string> invalid_transactions;
	while (transactions_to_process.size() > 0) {

		// find next one
		std::remove_reference_t<decltype(transactions.at(""))> transaction_to_process;
		try {
			transaction_to_process = [&](){
				std::set<std::string> transactions_to_remove;

				for (const auto& transaction_hex: transactions_to_process) {
					if (transactions.count(transaction_hex) == 0) {
						throw std::runtime_error(__FILE__ "(" + std::to_string(__LINE__) + ")");
					}

					const auto& transaction = transactions.at(transaction_hex);

					if (invalid_transactions.count(transaction.hash_hex) > 0) {
						if (verbose) {
							std::cout << "Skipping invalid transaction "
								<< transaction.hash_hex << std::endl;
						}
						continue;
					}

					if (invalid_transactions.count(transaction.previous_hex) > 0) {
						transactions_to_remove.insert(transaction.hash_hex);
						invalid_transactions.insert(transaction.hash_hex);
						if (verbose) {
							std::cout << "Marking child " << transaction.hash_hex
								<< " of invalid transaction " << transaction.previous_hex
								<< " as invalid" << std::endl;
						}
						continue;
					}

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
						if (invalid_transactions.count(transaction.send_hex) > 0) {
							transactions_to_remove.insert(transaction.hash_hex);
							invalid_transactions.insert(transaction.hash_hex);
							if (verbose) {
								std::cout << "Marking receive " << transaction.hash_hex
									<< " as invalid because matching send " << transaction.send_hex
									<< " is invalid" << std::endl;
							}
							continue;
						}

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
			std::cout << "Processing " << transaction_to_process.hash_hex << std::endl;
		}

		// receive
		if (transaction_to_process.send_hex.size() > 0) {

			if (invalid_transactions.count(transaction_to_process.send_hex) > 0) {
				if (verbose) {
					std::cout << "Receive " << transaction_to_process.hash_hex
						<< " is invalid because matched send is invalid: "
						<< transaction_to_process.send_hex << std::endl;
				}
				invalid_transactions.insert(transaction_to_process.hash_hex);
				transactions_to_process.erase(transaction_to_process.hash_hex);
				continue;
			}

			if (processed_transactions.count(transaction_to_process.send_hex) == 0) {
				std::cerr << "Send " << transaction_to_process.send_hex
					<< " not processed for receive" << std::endl;
				return EXIT_FAILURE;
			}

			const auto& send = processed_transactions.at(transaction_to_process.send_hex);

			if (processed_transactions.count(send.previous_hex) == 0) {
				std::cerr << "Prior transaction " << send.previous_hex
					<< " for send " << send.hash_hex << " not processed" << std::endl;
				return EXIT_FAILURE;
			}

			CryptoPP::Integer
				old_balance_sender(0l),
				new_balance_sender(0l),
				old_balance_receiver(0l);

			const auto old_balance_sender_bin = taraxa::hex2bin(
				processed_transactions.at(send.previous_hex).new_balance_hex
			);
			old_balance_sender.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(old_balance_sender_bin.data())),
				old_balance_sender_bin.size()
			);

			const auto new_balance_sender_bin = taraxa::hex2bin(send.new_balance_hex);
			new_balance_sender.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_sender_bin.data())),
				new_balance_sender_bin.size()
			);

			// continue from previous balance if not first transaction of receiving account
			if (transaction_to_process.previous_hex != "0000000000000000000000000000000000000000000000000000000000000000") {
				const auto& old_balance_receiver_bin = taraxa::hex2bin(
					transactions.at(transaction_to_process.previous_hex).new_balance_hex
				);
				old_balance_receiver.Decode(
					reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(old_balance_receiver_bin.data())),
					old_balance_receiver_bin.size()
				);
			}

			CryptoPP::Integer new_balance_receiver = old_balance_receiver + old_balance_sender - new_balance_sender;

			if (new_balance_receiver <= old_balance_receiver) {
				if (verbose) {
					std::cout << "Transaction " << transaction_to_process.hash_hex
						<< " is invalid, new balance of receiver is smaller than old: "
						<< old_balance_receiver << " -> " << new_balance_receiver << std::endl;
				}
				invalid_transactions.insert(transaction_to_process.hash_hex);
				transactions_to_process.erase(transaction_to_process.hash_hex);
				continue;
			}

			if (verbose) {
				const auto& pubkey_hex = transaction_to_process.pubkey_hex;
				std::cout << "Balance of " << pubkey_hex.substr(0, 5)
					<< "..." << pubkey_hex.substr(pubkey_hex.size() - 5)
					<< " before receive: " << old_balance_receiver
					<< ", balance after receive: " << new_balance_receiver << std::endl;
			}

			if (accounts.count(transaction_to_process.pubkey_hex) == 0) {
				std::cerr << "No account for receive transaction "
					<< transaction_to_process.hash_hex << std::endl;
				return EXIT_FAILURE;
			}

			auto& receiver = accounts.at(transaction_to_process.pubkey_hex);

			std::string new_balance_receiver_bin(8, 0);
			new_balance_receiver.Encode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_receiver_bin.data())),
				new_balance_receiver_bin.size()
			);

			receiver.balance_hex = taraxa::bin2hex(new_balance_receiver_bin);

			if (account_str.size() > 0 and account_str == receiver.pubkey_hex) {
				account_to_print = receiver;
			}
			if (transaction_str.size() > 0 and transaction_str == transaction_to_process.hash_hex) {
				transaction_to_print = transaction_to_process;
			}

		// send
		} else {

			CryptoPP::Integer old_balance(0l), new_balance(0l);

			const auto old_balance_bin = taraxa::hex2bin(
				transactions.at(transaction_to_process.previous_hex).new_balance_hex
			);
			old_balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(old_balance_bin.data())),
				old_balance_bin.size()
			);

			const auto new_balance_bin = taraxa::hex2bin(
				transaction_to_process.new_balance_hex
			);
			new_balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_bin.data())),
				new_balance_bin.size()
			);

			if (new_balance >= old_balance) {
				if (verbose) {
					std::cout << "Transaction " << transaction_to_process.hash_hex
						<< " is invalid, new balance of sender is not smaller than old: "
						<< old_balance << " -> " << new_balance << std::endl;
				}
				invalid_transactions.insert(transaction_to_process.hash_hex);
				transactions_to_process.erase(transaction_to_process.hash_hex);
				continue;
			}

			if (verbose) {
				const auto& pubkey_hex = transaction_to_process.pubkey_hex;
				std::cout << "Balance of " << pubkey_hex.substr(0, 5)
					<< "..." << pubkey_hex.substr(pubkey_hex.size() - 5)
					<< " before send: " << old_balance
					<< ", balance after send: " << new_balance << std::endl;
			}

			if (accounts.count(transaction_to_process.pubkey_hex) == 0) {
				std::cerr << "No account for send transaction "
					<< transaction_to_process.hash_hex << std::endl;
				return EXIT_FAILURE;
			}

			auto& sender = accounts.at(transaction_to_process.pubkey_hex);
			sender.balance_hex = transaction_to_process.new_balance_hex;

			if (account_str.size() > 0 and account_str == sender.pubkey_hex) {
				account_to_print = sender;
			}
			if (transaction_str.size() > 0 and transaction_str == transaction_to_process.hash_hex) {
				transaction_to_print = transaction_to_process;
			}
		}

		processed_transactions[transaction_to_process.hash_hex] = transaction_to_process;
		transactions_to_process.erase(transaction_to_process.hash_hex);
	}

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
			CryptoPP::Integer balance;
			const auto balance_bin = taraxa::hex2bin(account_to_print.balance_hex);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(balance_bin.data())),
				balance_bin.size()
			);
			std::cout << "Final balance of " << account_str.substr(0, 5)
				<< "..." << account_str.substr(account_str.size() - 5)
				<< ": " << balance << std::endl;
		}

		if (transaction_str.size() > 0) {
			if (processed_transactions.count(transaction_str) == 0) {
				std::cerr << "Transaction " << transaction_str.substr(0, 5)
					<< "..." << transaction_str.substr(transaction_str.size() - 5)
					<< " not found" << std::endl;
				return EXIT_FAILURE;
			}

			CryptoPP::Integer balance;
			const auto balance_bin = taraxa::hex2bin(transaction_to_print.new_balance_hex);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(balance_bin.data())),
				balance_bin.size()
			);
			account_str = transaction_to_print.pubkey_hex;
			const auto hash = transaction_to_print.hash_hex;
			std::cout << "Balance of " << account_str.substr(0, 5)
				<< "..." << account_str.substr(account_str.size() - 5)
				<< " at " << hash.substr(0, 5) << "..." << hash.substr(hash.size() - 5)
				<< ": " << balance << std::endl;
		}
	}
	return EXIT_SUCCESS;
}
