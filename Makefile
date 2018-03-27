PROGRAMS = generate_private_key print_key_info sign_message verify_message

COMPILE = @echo CC $@ && g++ -std=c++17 -O3 -W -Wall -Wextra -pedantic $< -o $@ -I submodules -lboost_program_options -L submodules/cryptopp -lcryptopp -DCRYPTOPP_DISABLE_ASM

HEADERS = bin2hex2bin.hpp signatures.hpp

all: dependencies $(PROGRAMS) $(HEADERS)

dependencies: submodules/cryptopp submodules/cryptopp/libcryptopp.a

submodules/cryptopp:
	@echo cryptopp submodule does not seem to exists, did you use --recursive in git clone?

submodules/cryptopp/libcryptopp.a: submodules/cryptopp
	@echo Attampting to compile cryptopp, if it fails try compiling it manually
	$(MAKE) -C submodules/cryptopp && touch $@

generate_private_key: generate_private_key.cpp Makefile
	$(COMPILE)

print_key_info: print_key_info.cpp Makefile
	$(COMPILE)

sign_message: sign_message.cpp Makefile
	$(COMPILE)

verify_message: verify_message.cpp Makefile
	$(COMPILE)

c: clean
clean:
	rm -f $(PROGRAMS)
