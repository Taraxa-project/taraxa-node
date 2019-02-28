# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -I submodules -I submodules/rapidjson/include -I . -I concur_storage -I grpc
OS := $(shell uname)
ifneq ($(OS), Darwin) #Mac
	CPPFLAGS += -DCRYPTOPP_DISABLE_ASM 
endif
CXXFLAGS := -std=c++17 -g -W -Wall -Wextra -pedantic
LDFLAGS := -L submodules/cryptopp
LIBS := -lboost_program_options -lboost_filesystem -lboost_system -lcryptopp
BUILD := build
TEST_BUILD := test_build
OBJ_DIR := obj

GOOGLE_APIS_OBJ := $(wildcard google/obj/*.o)
GOOGLE_APIS_FLAG := `pkg-config --cflags protobuf grpc++ --libs protobuf grpc++` -lgrpc++_reflection -ldl -I./grpc

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

create_build_dir: 	
	@if [ ! -d $(BUILD) ]; then	mkdir -p $(BUILD); fi 

create_test_dir: 
	@if [ ! -d $(TEST_BUILD) ]; then	mkdir -p $(TEST_BUILD); fi 

$(BUILD)/main: rocks_db.cpp state_block.cpp user_account.cpp util.cpp wallet.cpp rpc.cpp block_processor.cpp network.cpp udp_buffer.cpp full_node.cpp main.cpp $(DEPENDENCIES)
	$(COMPILE) -lrocksdb -lboost_thread-mt

$(TEST_BUILD)/dag_test: create_test_dir
	g++ -std=c++17 -o $(TEST_BUILD)/dag_test core_tests/dag_test.cpp types.cpp dag.cpp state_block.cpp util.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -I. 

$(TEST_BUILD)/network_test: create_test_dir 
	g++ -std=c++17 -o $(TEST_BUILD)/network_test core_tests/network_test.cpp rocks_db.cpp network.cpp udp_buffer.cpp util.cpp state_block.cpp full_node.cpp types.cpp visitor.cpp dag.cpp block_proposer.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -I. -lboost_system -lrocksdb
# make c; make core_tests/network_test; ./test_build/network_test

$(TEST_BUILD)/state_block_test: create_test_dir 
	g++ -std=c++17 -o $(TEST_BUILD)/state_block_test core_tests/state_block_test.cpp state_block.cpp util.cpp types.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -I.  
#

$(TEST_BUILD)/full_node_test: create_test_dir
	g++ -std=c++17 -o $(TEST_BUILD)/full_node_test core_tests/full_node_test.cpp rocks_db.cpp state_block.cpp util.cpp udp_buffer.cpp network.cpp full_node.cpp types.cpp visitor.cpp dag.cpp block_proposer.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -lboost_system -lrocksdb

$(TEST_BUILD)/concur_hash_test: create_test_dir
	g++ -std=c++17 -o $(TEST_BUILD)/concur_hash_test core_tests/concur_hash_test.cpp concur_storage/concur_hash.cpp concur_storage/conflict_detector.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -lboost_system

$(TEST_BUILD)/transaction_test: create_test_dir
	g++ -std=c++17 -o $(TEST_BUILD)/transaction_test core_tests/transaction_test.cpp transaction.cpp types.cpp util.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -lboost_system


main: create_build_dir
	g++ -std=c++17 -o $(BUILD)/main rocks_db.cpp state_block.cpp util.cpp udp_buffer.cpp network.cpp full_node.cpp types.cpp visitor.cpp dag.cpp block_proposer.cpp rpc.cpp main.cpp $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -lboost_system -lrocksdb -pthread

protoc_ledger: 
	protoc -I. --grpc_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/ledger.proto
	protoc -I. --cpp_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/ledger.proto

protoc_transaction: 
	protoc -I. --grpc_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/transaction.proto
	protoc -I. --cpp_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/transaction.proto  

grpc: create_test_dir protoc
	g++ -std=c++17 $(GOOGLE_APIS_FLAG) -o $(TEST_BUILD)/grpc_test grpc_server.cpp grpc/proto/ledger.grpc.pb.cc grpc/proto/ledger.pb.cc $(GOOGLE_APIS_OBJ) $(CPPFLAGS) $(LDFLAGS) $(LIBS) 

test: $(TEST_BUILD)/full_node_test $(TEST_BUILD)/state_block_test $(TEST_BUILD)/network_test $(TEST_BUILD)/dag_test $(TEST_BUILD)/concur_hash_test main

run_test: test
	./$(TEST_BUILD)/dag_test
	./$(TEST_BUILD)/network_test
	./$(TEST_BUILD)/state_block_test
	./$(TEST_BUILD)/full_node_test
	./$(TEST_BUILD)/concur_hash_test
	./$(TEST_BUILD)/transaction_test
ct:
	rm -rf $(CLEAN_TESTS)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILD) $(PROGRAMS) $(CLEAN_TESTS) $(TEST_BUILD)

.PHONY: run_test protoc grpc

