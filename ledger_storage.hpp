/*
Copyright 2018 Ilja Honkonen
*/

#ifndef LEDGER_STORAGE_HPP
#define LEDGER_STORAGE_HPP

#include "accounts.hpp"
#include "bin2hex2bin.hpp"
#include "signatures.hpp"
#include "transactions.hpp"

#include <boost/filesystem.hpp>
#include <cryptopp/blake2.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>


namespace taraxa {


//! Usually Path == boost::filesystem::path
template<class Path> Path get_vote_path(
	const std::string& hash_hex,
	const Path& votes_path
) {
	using std::to_string;

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (hash_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			"Hex format hash of vote must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(hash_hex.size())
		);
	}

	auto vote_path = votes_path;
	vote_path /= hash_hex.substr(0, 2);
	vote_path /= hash_hex.substr(2, 2);
	vote_path /= hash_hex.substr(4);

	return vote_path;
}


//! Usually Path == boost::filesystem::path
template<class Path> Path get_transaction_path(
	const std::string& hash_hex,
	const Path& transactions_path
) {
	using std::to_string;

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (hash_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			"Hex format hash of transaction must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(hash_hex.size())
		);
	}

	auto transaction_path = transactions_path;
	transaction_path /= hash_hex.substr(0, 2);
	transaction_path /= hash_hex.substr(2, 2);
	transaction_path /= hash_hex.substr(4);

	return transaction_path;
}


//! Usually Path == boost::filesystem::path
template<class Path> Path get_account_path(
	const std::string& pubkey_hex,
	const Path& accounts_path
) {
	using std::to_string;

	const auto pubkey_hex_size = 2 * taraxa::public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (pubkey_hex.size() != pubkey_hex_size) {
		throw std::invalid_argument(
			"Hex format public key must be " + to_string(pubkey_hex_size)
			+ " characters but is " + to_string(pubkey_hex.size())
		);
	}

	auto account_path = accounts_path;
	account_path /= pubkey_hex.substr(0, 2);
	account_path /= pubkey_hex.substr(2);

	return account_path;
}


template<
	class Hasher
> std::tuple<
	std::map<std::string, taraxa::Account<Hasher>>,
	std::map<std::string, taraxa::Transaction<Hasher>>,
	std::map<std::string, taraxa::Transient_Vote<Hasher>>
> load_ledger_data(
	const std::string& ledger_path_str,
	const bool verbose
) {
	using std::to_string;

	boost::filesystem::path ledger_path(ledger_path_str);
	if (ledger_path.size() > 0) {
		if (not boost::filesystem::exists(ledger_path)) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Ledger directory doesn't exist: " + ledger_path_str
			);
		}
		if (not boost::filesystem::is_directory(ledger_path)) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Ledger path isn't a directory: " + ledger_path_str
			);
		}
	}

	// load account data
	std::map<std::string, taraxa::Account<Hasher>> accounts;
	auto accounts_path = ledger_path;
	accounts_path /= "accounts";
	if (
		boost::filesystem::exists(accounts_path)
		and boost::filesystem::is_directory(accounts_path)
	) {
		if (verbose) {
			std::cout << "Loading accounts from " << accounts_path << std::endl;
		}

		for (const auto& account_path: boost::filesystem::recursive_directory_iterator(accounts_path)) {
			if (boost::filesystem::is_directory(account_path)) {
				continue;
			}

			taraxa::Account<Hasher> account;
			try {
				account.load(account_path.path().string(), verbose);
			} catch (...) {
				throw;
			}
			accounts[account.address_hex] = account;
		}
	}
	if (verbose) {
		std::cout << "Loaded " << accounts.size() << " accounts" << std::endl;
	}

	// load transaction data
	std::map<std::string, taraxa::Transaction<Hasher>> transactions;
	auto transactions_path = ledger_path;
	transactions_path /= "transactions";
	if (boost::filesystem::exists(transactions_path)) {
		if (verbose) {
			std::cout << "Loading transactions from " << transactions_path << std::endl;
		}

		for (const auto& transaction_path: boost::filesystem::recursive_directory_iterator(transactions_path)) {
			if (boost::filesystem::is_directory(transaction_path)) {
				continue;
			}

			taraxa::Transaction<Hasher> transaction;
			try {
				transaction.load(transaction_path.path().string(), verbose);
			} catch (...) {
				throw;
			}
			transactions[transaction.hash_hex] = transaction;
		}
	}
	if (verbose) {
		std::cout << "Loaded " << transactions.size() << " transactions" << std::endl;
	}

	// load vote data
	std::map<std::string, taraxa::Transient_Vote<Hasher>> votes;
	auto votes_path = ledger_path;
	votes_path /= "votes";
	if (boost::filesystem::exists(votes_path)) {
		if (verbose) {
			std::cout << "Loading votes from " << votes_path << std::endl;
		}

		for (const auto& vote_path: boost::filesystem::recursive_directory_iterator(votes_path)) {
			if (boost::filesystem::is_directory(vote_path)) {
				continue;
			}

			taraxa::Transient_Vote<Hasher> vote;
			try {
				vote.load(vote_path.path().string(), verbose);
			} catch (...) {
				throw;
			}
			votes[vote.hash_hex] = vote;
		}
	}
	if (verbose) {
		std::cout << "Loaded " << votes.size() << " votes" << std::endl;
	}

	return std::make_tuple(accounts, transactions, votes);
}


} // namespace taraxa

#endif // ifndef LEDGER_STORAGE_HPP
