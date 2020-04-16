/*
Copyright 2018 Ilja Honkonen
*/

/*
Header declares all block creation API
*/

#ifndef CREATE_RECEIVE_HPP
#define CREATE_RECEIVE_HPP
#include <string>

namespace taraxa{
std::string create_receive(
	const std::string & exp_hex, 
	const std::string & previous_hex,
	const std::string & send_hex
);
}
#endif
