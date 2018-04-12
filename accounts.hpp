/*
Copyright 2018 Ilja Honkonen
*/

#ifndef ACCOUNTS_HPP
#define ACCOUNTS_HPP

#include "bin2hex2bin.hpp"

#include <cryptopp/integer.h>

#include <stdexcept>
#include <string>


namespace taraxa {

class Account {
public:
	std::string pubkey_hex, address_hex, genesis_transaction_hex;
	CryptoPP::Integer balance;
};

} // namespace taraxa

#endif // ifndef ACCOUNTS_HPP
