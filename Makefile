# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -I submodules -I submodules/rapidjson/include -I .
OS := $(shell uname)
ifneq ($(OS), Darwin) #Mac
  CPPFLAGS += -DCRYPTOPP_DISABLE_ASM 
endif
CXXFLAGS := -std=c++17 -g -W -Wall -Wextra -pedantic
LDFLAGS := -L submodules/cryptopp
LIBS := -lboost_program_options -lboost_filesystem -lboost_system -lcryptopp
BUILD := build


PROGRAMS = \
    $(BUILD)/bin2hex \
    $(BUILD)/generate_private_key \
    $(BUILD)/generate_private_key_from_seed \
    $(BUILD)/print_key_info \
    $(BUILD)/sign_message \
    $(BUILD)/verify_message \
    $(BUILD)/create_send \
    $(BUILD)/create_receive \
    $(BUILD)/add_transaction \
    $(BUILD)/get_balance \
    $(BUILD)/create_transient_vote \
    $(BUILD)/add_vote \
    $(BUILD)/serialize_payloads \
    $(BUILD)/replace_transaction \
    $(BUILD)/vrf_participate \
    $(BUILD)/bls_sign_message \
    $(BUILD)/bls_print_key_info \
    $(BUILD)/bls_verify_signature \
    $(BUILD)/bls_make_threshold_keys \
    $(BUILD)/bls_merge_signatures \
    $(BUILD)/bls_merge_public_keys \
    $(BUILD)/bls_merge_secret_keys \
    $(BUILD)/sodium_generate_private_key_from_seed \
    $(BUILD)/sodium_get_vrf_proof \
    $(BUILD)/sodium_get_vrf_output \
    $(BUILD)/sodium_verify_vrf_proof \
    $(BUILD)/main

COMPILE = @echo CXX $@ && $(CXX) $(CXXFLAGS) $? -o $@ $(CPPFLAGS) $(LDFLAGS) $(LIBS)
BLS_COMPILE = $(COMPILE) -DMCLBN_FP_UNIT_SIZE=4 -I submodules/bls/include -I submodules/mcl/include -L submodules/bls/lib -lbls256 -L submodules/mcl/lib -lmcl -lgmp -lcrypto 

ifeq ($(OS), Darwin) #Mac
  BLS_COMPILE += -L /usr/local/Cellar/openssl@1.1/1.1.1/lib -I /usr/local/Cellar/openssl@1.1/1.1.1/include
endif
 
HEADERS = \
    accounts.hpp \
    balances.hpp \
    bin2hex2bin.hpp \
    hashes.hpp \
    ledger_storage.hpp \
    signatures.hpp \
    transactions.hpp \
    create_blocks.hpp \
    generate_private_key.hpp \
    generate_private_key_from_seed.hpp \
    full_node.hpp \
    state_block.hpp \
    user_account.hpp \
    rpc.hpp \
    util.hpp \
    wallet.hpp

all: create_dir $(DEPENDENCIES) $(PROGRAMS)

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

create_dir: 	
	@if [ ! -d $(BUILD) ]; then	mkdir -p $(BUILD); fi 

$(BUILD)/bin2hex: tests/bin2hex.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/generate_private_key: tests/generate_private_key.cpp generate_private_key.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/generate_private_key_from_seed: tests/generate_private_key_from_seed.cpp generate_private_key_from_seed.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/print_key_info: tests/print_key_info.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/sign_message: tests/sign_message.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/verify_message: tests/verify_message.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/create_send: tests/create_send.cpp create_send.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/create_receive: tests/create_receive.cpp create_receive.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/add_transaction: tests/add_transaction.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/get_balance: get_balance.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/create_transient_vote: tests/create_transient_vote.cpp create_transient_vote.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/add_vote: tests/add_vote.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/serialize_payloads: serialize_payloads.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/replace_transaction: replace_transaction.cpp $(DEPENDENCIES) 
	$(COMPILE)

$(BUILD)/vrf_participate: vrf_participate.cpp $(DEPENDENCIES) 
	$(COMPILE) $(shell pkg-config --cflags --libs "libcrypto >= 1.1") -Wno-class-memaccess

$(BUILD)/submodules/bls/lib/libbsl256.a: submodules/bls/include/bls/bls.h
	$(MAKE) -C submodules/bls

$(BUILD)/bls_sign_message: bls_sign_message.cpp $(DEPENDENCIES) 
	$(BLS_COMPILE)

$(BUILD)/bls_print_key_info: bls_print_key_info.cpp $(DEPENDENCIES) 
	$(BLS_COMPILE)

$(BUILD)/bls_verify_signature: bls_verify_signature.cpp $(DEPENDENCIES) 
	$(BLS_COMPILE)

$(BUILD)/bls_make_threshold_keys: bls_make_threshold_keys.cpp $(DEPENDENCIES) 
	$(BLS_COMPILE)

$(BUILD)/bls_merge_signatures: bls_merge_signatures.cpp $(DEPENDENCIES) 
	$(BLS_COMPILE)

$(BUILD)/bls_merge_public_keys: bls_merge_public_keys.cpp $(DEPENDENCIES)  
	$(BLS_COMPILE)

$(BUILD)/bls_merge_secret_keys: bls_merge_secret_keys.cpp $(DEPENDENCIES)
	$(BLS_COMPILE)

$(BUILD)/sodium_generate_private_key_from_seed: sodium_generate_private_key_from_seed.cpp 
	$(COMPILE) `pkg-config libsodium --cflags --libs` || echo Do you have Algorand version of libsodium installed from https://github.com/algorand/libsodium?

$(BUILD)/sodium_get_vrf_proof: sodium_get_vrf_proof.cpp $(DEPENDENCIES) 
	$(COMPILE) `pkg-config libsodium --cflags --libs`

$(BUILD)/sodium_get_vrf_output: sodium_get_vrf_output.cpp $(DEPENDENCIES) 
	$(COMPILE) `pkg-config libsodium --cflags --libs`

$(BUILD)/sodium_verify_vrf_proof: sodium_verify_vrf_proof.cpp $(DEPENDENCIES) 
	$(COMPILE) `pkg-config libsodium --cflags --libs`

$(BUILD)/main: create_send.cpp rocks_db.cpp state_block.cpp user_account.cpp util.cpp wallet.cpp  rpc.cpp block_processor.cpp full_node.cpp main.cpp $(DEPENDENCIES)
	$(COMPILE) -lrocksdb -lboost_thread-mt

TESTS =
CLEAN_TESTS =
include \
    tests/generate_private_key_from_seed/test_include \
    tests/add_transaction/test1/test_include \
    tests/add_transaction/test2/test_include \
    tests/get_balance/test1/test_include \
    tests/replace_transaction/test1/test_include \
    tests/replace_transaction/test2/test_include \
    tests/vote_delegation/test1/test_include \
    tests/bls_sign/test1/test_include \
    tests/bls_verify/test1/test_include \
    tests/bls_merge/pubkey1/test_include \
    tests/bls_merge/signature1/test_include

t: test
test: $(TESTS)

ct:
	rm -rf $(CLEAN_TESTS)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILD) $(PROGRAMS) $(CLEAN_TESTS)
