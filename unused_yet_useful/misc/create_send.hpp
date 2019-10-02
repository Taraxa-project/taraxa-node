/*
Copyright 2018 Ilja Honkonen
*/

/*
Header declares all block creation API
*/

#ifndef CREATE_SEND_HPP
#define CREATE_SEND_HPP
#include <string>

namespace taraxa{
std::string create_send(
	const std::string &exp_hex, 
	const std::string &receiver_hex, 
	const std::string &new_balance_str, 
	const std::string &payload_hex, 
	const std::string &previous_hex
);
}
#endif
