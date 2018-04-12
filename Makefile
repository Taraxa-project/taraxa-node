# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -DCRYPTOPP_DISABLE_ASM -I submodules -I submodules/rapidjson/include
CXXFLAGS := -std=c++17 -O3 -W -Wall -Wextra -pedantic
LDFLAGS := -L submodules/cryptopp
LIBS := -lboost_program_options -lboost_filesystem -lboost_system -lcryptopp


PROGRAMS = \
    generate_private_key \
    print_key_info \
    sign_message \
    verify_message \
    create_send \
    create_receive \
    append_to_ledger

COMPILE = @echo CXX $@ && $(CXX) $(CXXFLAGS) $< -o $@ $(CPPFLAGS) $(LDFLAGS) $(LIBS)

HEADERS = bin2hex2bin.hpp signatures.hpp

all: $(DEPENDENCIES) $(PROGRAMS)

DEPENDENCIES: \
    submodules/cryptopp/Readme.txt \
    submodules/cryptopp/libcryptopp.a \
    submodules/rapidjson/readme.md

submodules/cryptopp/Readme.txt:
	@echo cryptopp submodule does not seem to exists, did you use --recursive in git clone? && exit 1

submodules/cryptopp/libcryptopp.a: submodules/cryptopp/Readme.txt
	@echo Attempting to compile cryptopp, if it fails try compiling it manually
	$(MAKE) -C submodules/cryptopp

submodules/rapidjson/readme.md:
	@echo rapidjson submodule does not seem to exists, did you use --recursive in git clone? && exit 1

generate_private_key: generate_private_key.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

print_key_info: print_key_info.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

sign_message: sign_message.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

verify_message: verify_message.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

create_send: create_send.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

create_receive: create_receive.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

append_to_ledger: append_to_ledger.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

c: clean
clean:
	rm -f $(PROGRAMS)


TESTS = \
    tests/append_to_ledger/test1/accounts/6B/17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C2964FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5 \
    tests/append_to_ledger/test1/transactions/A6/1A/5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE

tests/append_to_ledger/test1/accounts/6B/17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C2964FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5: tests/append_to_ledger/test1/transactions/A6/1A/5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE

tests/append_to_ledger/test1/transactions/A6/1A/5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE: append_to_ledger
	./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/01_genesis


t: test
test: $(TESTS)
