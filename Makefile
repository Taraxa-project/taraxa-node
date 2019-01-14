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
TEST_BUILD := test_build

PROGRAMS = \
    $(BUILD)/main

COMPILE = @echo CXX $@ && $(CXX) $(CXXFLAGS) $? -o $@ $(CPPFLAGS) $(LDFLAGS) $(LIBS)
TEST_COMPILE = echo CXX $@ && $(CXX) $(CXXFLAGS) $? -o $(TEST_BUILD)/run_test $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest

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
    wallet.hpp \
    block_processor.hpp

TESTS = \
    core_tests/dag_test.cpp

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

create_test_dir: 
	@if [ ! -d $(BUILD) ]; then	mkdir -p $(TEST_BUILD); fi 

$(BUILD)/main: rocks_db.cpp state_block.cpp user_account.cpp util.cpp wallet.cpp  rpc.cpp block_processor.cpp network.cpp full_node.cpp main.cpp $(DEPENDENCIES)
	$(COMPILE) -lrocksdb -lboost_thread-mt

core_tests/dag_test:  
	g++ -std=c++17 core_tests/dag_test.cpp dag.cpp -lgtest -I.

core_tests/network_test: create_test_dir 
	g++ -std=c++17 -o $(TEST_BUILD)/network_test core_tests/network_test.cpp network.cpp util.cpp $(CPPFLAGS) -lgtest -lboost_thread-mt -I. -lboost_system 
# make c; make core_tests/network_test; ./test_build/network_test

core_tests/state_block_test: create_test_dir 
	g++ -std=c++17 -o $(TEST_BUILD)/state_block_test core_tests/state_block_test.cpp state_block.cpp util.cpp $(CPPFLAGS) -lgtest -I. -lboost_system 
#

core_tests/full_node_test:  
	g++ -std=c++17 -o core_tests/full_node_test.cpp rocks_db.cpp state_block.cpp user_account.cpp util.cpp block_processor.cpp network.cpp full_node.cpp $(CPPFLAGS) -lgtest -lboost_thread-mt -lboost_system -lrocksdb


test: 
	 
ct:
	rm -rf $(CLEAN_TESTS)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILD) $(PROGRAMS) $(CLEAN_TESTS) $(TEST_BUILD)

