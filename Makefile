# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -DCRYPTOPP_DISABLE_ASM -I submodules -I submodules/rapidjson/include
CXXFLAGS := -std=c++17 -O3 -W -Wall -Wextra -pedantic
LDFLAGS := -L submodules/cryptopp
LIBS := -lboost_program_options -lcryptopp


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

all: dependencies $(PROGRAMS)

dependencies: \
    submodules/cryptopp/Readme.txt \
    submodules/cryptopp/libcryptopp.a \
    submodules/rapidjson \
    submodules/rapidjson/include/rapidjson/document.h

submodules/cryptopp/Readme.txt:
	@echo cryptopp submodule does not seem to exists, did you use --recursive in git clone? && exit 1

submodules/cryptopp/libcryptopp.a: submodules/cryptopp
	@echo Attempting to compile cryptopp, if it fails try compiling it manually
	$(MAKE) -C submodules/cryptopp && touch $@

submodules/rapidjson:
	@echo rapidjson submodule does not seem to exists, did you use --recursive in git clone? && exit 1

submodules/rapidjson/include/rapidjson/document.h: submodules/rapidjson
	@touch $@

generate_private_key: generate_private_key.cpp $(HEADERS) dependencies Makefile
	$(COMPILE)

print_key_info: print_key_info.cpp $(HEADERS) dependencies Makefile
	$(COMPILE)

sign_message: sign_message.cpp $(HEADERS) dependencies Makefile
	$(COMPILE)

verify_message: verify_message.cpp $(HEADERS) dependencies Makefile
	$(COMPILE)

create_send: create_send.cpp $(HEADERS) dependencies Makefile
	$(COMPILE)

create_receive: create_receive.cpp $(HEADERS) dependencies Makefile
	$(COMPILE)

append_to_ledger: append_to_ledger.cpp $(HEADERS) dependencies Makefile
	$(COMPILE) -lboost_filesystem -lboost_system

c: clean
clean:
	rm -f $(PROGRAMS)
