PROGRAMS = generate_private_key print_key_info

all: dependencies $(PROGRAMS)

dependencies: submodules/cryptopp submodules/cryptopp/libcryptopp.a

submodules/cryptopp:
	@echo cryptopp submodule does not seem to exists, did you use --recursive in git clone?

submodules/cryptopp/libcryptopp.a: submodules/cryptopp
	@echo Attempting to compile cryptopp, if it fails try compiling it manually
	$(MAKE) -C submodules/cryptopp && touch $@

generate_private_key: generate_private_key.cpp Makefile
	g++ -O3 -W -Wall -Wextra -pedantic $< -o $@ -I submodules -lboost_program_options -L submodules/cryptopp -lcryptopp -DCRYPTOPP_DISABLE_ASM

print_key_info: print_key_info.cpp Makefile
	g++ -O3 -W -Wall -Wextra -pedantic $< -o $@ -I submodules -lboost_program_options -L submodules/cryptopp -lcryptopp -DCRYPTOPP_DISABLE_ASM

c: clean
clean:
	rm -f $(PROGRAMS)
