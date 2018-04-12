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
	rm -rf $(PROGRAMS) tests/append_to_ledger/test1/accounts tests/append_to_ledger/test1/transactions signature_fail1.ok signature_fail2.ok signature_fail3.ok


TESTS = tests/append_to_ledger/test1/transactions/15/CA/69743C3060E3F4C47512B0561694B3558C2871DDCB24625DC249F3918092

tests/append_to_ledger/test1/accounts/6B/17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C2964FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5: tests/append_to_ledger/test1/transactions/A6/1A/5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE

tests/append_to_ledger/test1/transactions/A6/1A/5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE: append_to_ledger tests/append_to_ledger/test1/01_genesis
	@echo -n "TEST 01_genesis... " && ./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/01_genesis && echo PASS

signature_fail1.ok: tests/append_to_ledger/test1/02_signature_fail tests/append_to_ledger/test1/transactions/A6/1A/5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE
	@echo -n "TEST 02_signature_fail... " && ./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/02_signature_fail 2> /dev/null || touch signature_fail1.ok && echo PASS

signature_fail2.ok: signature_fail1.ok
	@echo -n "TEST 03_signature_fail... " && ./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/03_signature_fail 2> /dev/null || touch signature_fail2.ok && echo PASS

signature_fail3.ok: signature_fail2.ok
	@echo -n "TEST 04_signature_fail... " && ./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/04_signature_fail 2> /dev/null || touch signature_fail3.ok && echo PASS

tests/append_to_ledger/test1/transactions/04/39/54BA0E0FFD6972E2410C443AE672B66DFB2DF72FF1B165AC5A21CABF0F5E: signature_fail3.ok
	@echo -n "TEST 05_send1... " && ./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/05_send1 && echo PASS

tests/append_to_ledger/test1/transactions/15/CA/69743C3060E3F4C47512B0561694B3558C2871DDCB24625DC249F3918092: tests/append_to_ledger/test1/transactions/04/39/54BA0E0FFD6972E2410C443AE672B66DFB2DF72FF1B165AC5A21CABF0F5E
	@echo -n "TEST 06_receive1... " && ./append_to_ledger --ledger-path tests/append_to_ledger/test1 < tests/append_to_ledger/test1/06_receive1 && echo PASS

t: test
test: $(TESTS)
