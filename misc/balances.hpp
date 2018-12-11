/*
Copyright 2018 Ilja Honkonen
*/

#ifndef BALANCES_HPP
#define BALANCES_HPP

#include "accounts.hpp"
#include "bin2hex2bin.hpp"
#include "transactions.hpp"

#include <array>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>


namespace taraxa {

/*!
Updates current and final balances of @accounts.

Updates the final balance of each account in @accounts.
Updates the current balance of each account after every transaction
in @transactions.
*/
template <
	class Hasher
> void update_balances(
	std::map<std::string, Account<Hasher>>& accounts,
	std::map<std::string, Transaction<Hasher>>& transactions,
	const bool verbose
) {
	using std::to_string;

	/*
	Process genesis transactions without corresponding sends
	*/
	std::map<std::string, Transaction<Hasher>> processed_transactions;

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
			throw std::logic_error(__FILE__ "(" + to_string(__LINE__) + ")");
		}

		auto& account = accounts.at(transaction.pubkey_hex);
		if (verbose) {
			const auto balance_bin = hex2bin(transaction.new_balance_hex);
			CryptoPP::Integer balance(0l);
			balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(
					balance_bin.data())),
				balance_bin.size()
			);
			std::cout << "Setting initial balance of account "
				<< account.address_hex.substr(0, 5) << "..."
				<< account.address_hex.substr(account.address_hex.size() - 5)
				<< " to " << balance << std::endl;
		}

		if (transaction.payload_hex.size() > 0) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				" Genesis transaction cannot have a payload: "
				+ transaction.payload_hex
			);
		}

		account.balance_hex = transaction.new_balance_hex;
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

	std::map<std::string, std::string> new_vote_delegates;

	std::set<std::string> invalid_transactions;
	while (transactions_to_process.size() > 0) {

		// find next one
		Transaction<Hasher> transaction_to_process;
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
				throw std::logic_error(
					__FILE__ "(" + to_string(__LINE__) + ") Send "
					+ transaction_to_process.send_hex + " not processed for receive"
				);
			}

			const auto& send = processed_transactions.at(transaction_to_process.send_hex);

			if (processed_transactions.count(send.previous_hex) == 0) {
				throw std::logic_error(
					__FILE__ "(" + to_string(__LINE__) + ") Prior transaction "
					+ send.previous_hex + " for send " + send.hash_hex + " not processed"
				);
			}

			CryptoPP::Integer
				old_balance_sender(0l),
				new_balance_sender(0l),
				old_balance_receiver(0l);

			const auto old_balance_sender_bin = hex2bin(
				processed_transactions.at(send.previous_hex).new_balance_hex
			);
			old_balance_sender.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(
					old_balance_sender_bin.data())),
				old_balance_sender_bin.size()
			);

			const auto new_balance_sender_bin = hex2bin(send.new_balance_hex);
			new_balance_sender.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(
					new_balance_sender_bin.data())),
				new_balance_sender_bin.size()
			);

			// continue from previous balance if not first transaction of receiving account
			if (transaction_to_process.previous_hex != "0000000000000000000000000000000000000000000000000000000000000000") {
				const auto& old_balance_receiver_bin = hex2bin(
					transactions.at(transaction_to_process.previous_hex).new_balance_hex
				);
				old_balance_receiver.Decode(
					reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(
						old_balance_receiver_bin.data())),
					old_balance_receiver_bin.size()
				);
			}

			CryptoPP::Integer new_balance_receiver
				= old_balance_receiver
				+ old_balance_sender
				- new_balance_sender;

			if (new_balance_receiver < old_balance_receiver) {
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
				throw std::logic_error(
					__FILE__ "(" + to_string(__LINE__) + ") "
					+ "No account for receive transaction "
					+ transaction_to_process.hash_hex
				);
			}

			auto& receiver = accounts.at(transaction_to_process.pubkey_hex);

			// check if vote delegation has been accepted
			for (auto& item: new_vote_delegates) {
				if (item.second != transaction_to_process.send_hex) {
					continue;
				}

				item.second = receiver.pubkey_hex;
				break;
			}

			std::string new_balance_receiver_bin(8, 0);
			new_balance_receiver.Encode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(
					new_balance_receiver_bin.data())),
				new_balance_receiver_bin.size()
			);

			receiver.balance_hex = bin2hex(new_balance_receiver_bin);
			transaction_to_process.new_balance_hex = receiver.balance_hex;

		// send
		} else {

			CryptoPP::Integer old_balance(0l), new_balance(0l);

			const auto old_balance_bin = hex2bin(
				transactions.at(transaction_to_process.previous_hex).new_balance_hex
			);
			old_balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(old_balance_bin.data())),
				old_balance_bin.size()
			);

			const auto new_balance_bin = hex2bin(
				transaction_to_process.new_balance_hex
			);
			new_balance.Decode(
				reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_bin.data())),
				new_balance_bin.size()
			);

			if (new_balance > old_balance) {
				if (verbose) {
					std::cout << "Transaction " << transaction_to_process.hash_hex
						<< " is invalid, new balance of sender is larger than old: "
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
				throw std::logic_error(
					__FILE__ "(" + to_string(__LINE__) + ") "
					+ "No account for send transaction "
					+ transaction_to_process.hash_hex
				);
			}

			auto& sender = accounts.at(transaction_to_process.pubkey_hex);

			const std::string payload_bin = hex2bin(transaction_to_process.payload_hex);
			if (payload_bin == "delegate_vote") {
				// wait for corresponding receive
				new_vote_delegates[sender.pubkey_hex] = transaction_to_process.hash_hex;
			}

			sender.balance_hex = transaction_to_process.new_balance_hex;
		}

		processed_transactions[transaction_to_process.hash_hex] = transaction_to_process;
		transactions_to_process.erase(transaction_to_process.hash_hex);
	}

	transactions = processed_transactions;

	// update vote delegates
	for (const auto& item: new_vote_delegates) {
		if (accounts.count(item.first) == 0) {
			throw std::runtime_error(__FILE__ "(" + to_string(__LINE__) + ")");
		}
		if (accounts.count(item.second) == 0) {
			throw std::runtime_error(__FILE__ "(" + to_string(__LINE__) + ")");
		}

		accounts.at(item.first).vote_delegate_pubkey_hex = item.second;
	}
}

} // namespace taraxa

#endif // ifndef BALANCES_HPP
