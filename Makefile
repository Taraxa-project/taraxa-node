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
    append_to_ledger \
    get_balance \
    create_transient_vote

COMPILE = @echo CXX $@ && $(CXX) $(CXXFLAGS) $< -o $@ $(CPPFLAGS) $(LDFLAGS) $(LIBS)

HEADERS = \
    accounts.hpp \
    bin2hex2bin.hpp \
    hashes.hpp \
    ledger_storage.hpp \
    signatures.hpp \
    transactions.hpp

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

get_balance: get_balance.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

create_transient_vote: create_transient_vote.cpp $(HEADERS) $(DEPENDENCIES) Makefile
	$(COMPILE)

TESTS =
CLEAN_TESTS =
include \
    tests/append_to_ledger/test_include \
    tests/get_balance/test_include

t: test
test: $(TESTS)

c: clean
clean:
	rm -rf $(PROGRAMS) $(CLEAN_TESTS)
