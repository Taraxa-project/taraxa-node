# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -I submodules -I submodules/rapidjson/include -I submodules/secp256k1/include -I submodules/libff -I submodules/libff/libff -I submodules/ethash/include -I . -I concur_storage -I grpc -DBOOST_LOG_DYN_LINK 
OS := $(shell uname)
LOG_LIB = -lboost_log-mt
ifneq ($(OS), Darwin) #Mac
	CPPFLAGS += -DCRYPTOPP_DISABLE_ASM 
	LOG_LIB = -lboost_log
endif
CXXFLAGS := -std=c++17 -c -g -MMD -MP -MF
CXXFLAGS2 := -std=c++17 -c -g -MMD -MP -MF
LDFLAGS := -L submodules/cryptopp -L submodules/ethash/build/lib/ethash -L submodules/libff/build/libff -L submodules/secp256k1/.libs
LIBS := -DBOOST_LOG_DYN_LINK $(LOG_LIB) -lleveldb -lrocksdb -lsecp256k1 -lgmp -lscrypt -lpthread -lboost_program_options -lboost_filesystem -lboost_system -lboost_log_setup -lboost_log -lcryptopp -lethash -lff -lgtest -lboost_thread-mt -lrocksdb
BUILDDIR := build
TESTBUILDDIR := test_build
OBJECTDIR := obj
MKDIR := mkdir
RM := rm -f

COMPILE = $(CXX) $(CXXFLAGS)

GOOGLE_APIS_OBJ := $(wildcard google/obj/*.o)
GOOGLE_APIS_FLAG := `pkg-config --cflags protobuf grpc++ --libs protobuf grpc++` -lgrpc++_reflection -ldl -I./grpc

.depcheck-impl:
	@echo "DEPFILES=\$$(wildcard \$$(addsuffix .d, \$${OBJECTFILES} ))" >.dep.inc; \
	echo "DEPFILES+=\$$(wildcard \$$(addsuffix .d, \$${P2POBJECTFILES} ))" >>.dep.inc; \
	echo "DEPFILES+=\$$(wildcard \$$(addsuffix .d, \$${MAINOBJECTFILES} ))" >>.dep.inc; \
	echo "ifneq (\$${DEPFILES},)" >>.dep.inc; \
	echo "include \$${DEPFILES}" >>.dep.inc; \
	echo "endif" >>.dep.inc; \

main: $(BUILDDIR)/main
	@echo MAIN

all: $(DEPENDENCIES) main

include p2p.inc


OBJECTFILES= \
	${OBJECTDIR}/rocks_db.o \
	${OBJECTDIR}/dag_block.o \
	${OBJECTDIR}/util.o \
	${OBJECTDIR}/udp_buffer.o \
	${OBJECTDIR}/network.o \
	${OBJECTDIR}/full_node.o \
	${OBJECTDIR}/types.o \
	${OBJECTDIR}/visitor.o \
	${OBJECTDIR}/dag.o \
	${OBJECTDIR}/block_proposer.o \
	${OBJECTDIR}/rpc.o \
	${OBJECTDIR}/grpc_client.o \
	${OBJECTDIR}/grpc_server.o \
	${OBJECTDIR}/grpc_util.o \
	${OBJECTDIR}/transaction.o \
	${OBJECTDIR}/executor.o \
	${OBJECTDIR}/pbft_chain.o \
	${OBJECTDIR}/taraxa_grpc.pb.o \
	${OBJECTDIR}/taraxa_grpc.grpc.pb.o \
	${OBJECTDIR}/taraxa_capability.o \
	${OBJECTDIR}/account.o \
	${OBJECTDIR}/log_entry.o \
	${OBJECTDIR}/transaction_receipt.o \
	${OBJECTDIR}/state.o

MAINOBJECTFILES= \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/p2p_test.o \
	${OBJECTDIR}/dag_test.o \
	${OBJECTDIR}/network_test.o \
	${OBJECTDIR}/dag_block_test.o \
	${OBJECTDIR}/full_node_test.o \
	${OBJECTDIR}/concur_hash_test.o \
	${OBJECTDIR}/transaction_test.o \
	${OBJECTDIR}/grpc_test.o \
	${OBJECTDIR}/memorydb_test.o \
	${OBJECTDIR}/overlaydb_test.o \
	${OBJECTDIR}/statecachedb_test.o \
	${OBJECTDIR}/trie_test.o \
	${OBJECTDIR}/state_test.o

${OBJECTDIR}/taraxa_grpc.pb.o: grpc/proto/taraxa_grpc.pb.cc
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/taraxa_grpc.pb.o grpc/proto/taraxa_grpc.pb.cc $(CPPFLAGS)

${OBJECTDIR}/taraxa_grpc.grpc.pb.o: grpc/proto/taraxa_grpc.grpc.pb.cc
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/taraxa_grpc.grpc.pb.o grpc/proto/taraxa_grpc.grpc.pb.cc $(CPPFLAGS)

${OBJECTDIR}/rocks_db.o: rocks_db.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/rocks_db.o rocks_db.cpp $(CPPFLAGS)
	
${OBJECTDIR}/executor.o: executor.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/executor.o executor.cpp $(CPPFLAGS)
	
${OBJECTDIR}/pbft_chain.o: pbft_chain.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_chain.o pbft_chain.cpp $(CPPFLAGS)
	
${OBJECTDIR}/dag_block.o: dag_block.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/dag_block.o dag_block.cpp $(CPPFLAGS)
	
${OBJECTDIR}/util.o: util.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/util.o util.cpp $(CPPFLAGS)
	
${OBJECTDIR}/udp_buffer.o: udp_buffer.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/udp_buffer.o udp_buffer.cpp $(CPPFLAGS)
	
${OBJECTDIR}/network.o: network.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/network.o network.cpp $(CPPFLAGS)
	
${OBJECTDIR}/full_node.o: full_node.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/full_node.o full_node.cpp $(CPPFLAGS)
	
${OBJECTDIR}/types.o: types.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/types.o types.cpp $(CPPFLAGS)
	
${OBJECTDIR}/visitor.o: visitor.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/visitor.o visitor.cpp $(CPPFLAGS)
	
${OBJECTDIR}/dag.o: dag.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/dag.o dag.cpp $(CPPFLAGS)
	
${OBJECTDIR}/block_proposer.o: block_proposer.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/block_proposer.o block_proposer.cpp $(CPPFLAGS)
	
${OBJECTDIR}/rpc.o: rpc.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/rpc.o rpc.cpp $(CPPFLAGS)

${OBJECTDIR}/concur_hash.o: concur_storage/concur_hash.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/concur_hash.o concur_storage/concur_hash.cpp $(CPPFLAGS)

${OBJECTDIR}/conflict_detector.o: concur_storage/conflict_detector.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/conflict_detector.o concur_storage/conflict_detector.cpp $(CPPFLAGS)

${OBJECTDIR}/grpc_client.o: grpc_client.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/grpc_client.o grpc_client.cpp $(CPPFLAGS)

${OBJECTDIR}/grpc_server.o: grpc_server.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/grpc_server.o grpc_server.cpp $(CPPFLAGS)

${OBJECTDIR}/grpc_util.o: grpc_util.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/grpc_util.o grpc_util.cpp $(CPPFLAGS)

${OBJECTDIR}/transaction.o: transaction.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/transaction.o transaction.cpp $(CPPFLAGS)

# required for Account
${OBJECTDIR}/address.o: libdevcore/Address.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/address.o libdevcore/Address.cpp $(CPPFLAGS)

${OBJECTDIR}/account.o: libethereum/Account.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/account.o libethereum/Account.cpp $(CPPFLAGS)

# required for TransactionReceipt
${OBJECTDIR}/log_entry.o: libethcore/LogEntry.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/log_entry.o libethcore/LogEntry.cpp $(CPPFLAGS)

${OBJECTDIR}/transaction_receipt.o: libethereum/TransactionReceipt.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/transaction_receipt.o libethereum/TransactionReceipt.cpp $(CPPFLAGS)

#${OBJECTDIR}/common.o: libethcore/Common.cpp
	#${MKDIR} -p ${OBJECTDIR}
	#${RM} "$@.d"
	#${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/common.o libethcore/Common.cpp $(CPPFLAGS)

${OBJECTDIR}/state.o: libethereum/State.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/state.o libethereum/State.cpp $(CPPFLAGS)

${OBJECTDIR}/taraxa_capability.o: taraxa_capability.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/taraxa_capability.o taraxa_capability.cpp $(CPPFLAGS)

${OBJECTDIR}/main.o: main.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/main.o main.cpp $(CPPFLAGS)

${OBJECTDIR}/dag_test.o: core_tests/dag_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/dag_test.o core_tests/dag_test.cpp $(CPPFLAGS)

${OBJECTDIR}/network_test.o: core_tests/network_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/network_test.o core_tests/network_test.cpp $(CPPFLAGS)

${OBJECTDIR}/dag_block_test.o: core_tests/dag_block_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/dag_block_test.o core_tests/dag_block_test.cpp $(CPPFLAGS)	

${OBJECTDIR}/full_node_test.o: core_tests/full_node_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/full_node_test.o core_tests/full_node_test.cpp $(CPPFLAGS)	

${OBJECTDIR}/concur_hash_test.o: core_tests/concur_hash_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/concur_hash_test.o core_tests/concur_hash_test.cpp $(CPPFLAGS)

${OBJECTDIR}/p2p_test.o: core_tests/p2p_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/p2p_test.o core_tests/p2p_test.cpp $(CPPFLAGS)

${OBJECTDIR}/transaction_test.o: core_tests/transaction_test.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/transaction_test.o core_tests/transaction_test.cpp $(CPPFLAGS)

${OBJECTDIR}/grpc_test.o: core_tests/grpc_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/grpc_test.o core_tests/grpc_test.cpp $(CPPFLAGS)

${OBJECTDIR}/memorydb_test.o: core_tests/memorydb_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/memorydb_test.o core_tests/memorydb_test.cpp $(CPPFLAGS)

${OBJECTDIR}/overlaydb_test.o: core_tests/overlaydb_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/overlaydb_test.o core_tests/overlaydb_test.cpp $(CPPFLAGS)

${OBJECTDIR}/statecachedb_test.o: core_tests/statecachedb_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/statecachedb_test.o core_tests/statecachedb_test.cpp $(CPPFLAGS)

# required for trie_test
${OBJECTDIR}/mem_trie.o: crypto_tests/MemTrie.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/mem_trie.o crypto_tests/MemTrie.cpp $(CPPFLAGS)

${OBJECTDIR}/trie_test.o: crypto_tests/trie_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/trie_test.o crypto_tests/trie_test.cpp $(CPPFLAGS)

DEPENDENCIES = submodules/cryptopp/libcryptopp.a \
	submodules/ethash/build/lib/ethash/libethash.a \
	submodules/libff/build/libff/libff.a \
	submodules/secp256k1/.libs/libsecp256k1.a \
	core_tests/create_samples.hpp

submodules/cryptopp/libcryptopp.a:
	@echo Attempting to compile cryptopp, if it fails try compiling it manually

	@if [ $(OS) = 'Darwin' ]; then $(MAKE) -C submodules/cryptopp; else $(MAKE) -C submodules/cryptopp CXXFLAGS="-DNDEBUG -g2 -O3 -fPIC -DCRYPTOPP_DISABLE_ASM -pthread -pipe -c"; fi

submodules/ethash/build/lib/ethash/libethash.a:
	@echo Attempting to compile ethash, if it fails try compiling it manually
	cd submodules/ethash; ${MKDIR} -p build
	cd submodules/ethash/build; cmake ..; cmake --build .

submodules/libff/build/libff/libff.a:
	@echo Attempting to compile libff, if it fails try compiling it manually
	cd submodules/libff; ${MKDIR} -p build
	cd submodules/libff/build; cmake .. -DWITH_PROCPS=Off -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=c++ -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DOPENSSL_LIBRARIES=/usr/local/opt/openssl/lib; make

submodules/secp256k1/.libs/libsecp256k1.a:
	@echo Attempting to compile libsecp256k1, if it fails try compiling it manually
	cd submodules/secp256k1; ./autogen.sh
	cd submodules/secp256k1; ./configure --disable-shared --disable-tests --disable-coverage --disable-openssl-tests --disable-exhaustive-tests --disable-jni --with-bignum=no --with-field=64bit --with-scalar=64bit --with-asm=no --enable-module-ecdh --enable-module-recovery --enable-experimental 
	cd submodules/secp256k1; make

$(BUILDDIR)/main: $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES) $(OBJECTDIR)/main.o
	${MKDIR} -p ${BUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/main.o -o $(BUILDDIR)/main $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/dag_test: $(OBJECTDIR)/dag_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/dag_test.o -o $(TESTBUILDDIR)/dag_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/network_test: $(OBJECTDIR)/network_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/network_test.o -o $(TESTBUILDDIR)/network_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/dag_block_test: $(OBJECTDIR)/dag_block_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/dag_block_test.o -o $(TESTBUILDDIR)/dag_block_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/full_node_test: $(OBJECTDIR)/full_node_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/full_node_test.o -o $(TESTBUILDDIR)/full_node_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/concur_hash_test: $(OBJECTDIR)/concur_hash_test.o ${OBJECTDIR}/concur_hash.o ${OBJECTDIR}/conflict_detector.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/concur_hash_test.o ${OBJECTDIR}/concur_hash.o ${OBJECTDIR}/conflict_detector.o -o $(TESTBUILDDIR)/concur_hash_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/p2p_test: $(OBJECTDIR)/p2p_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/p2p_test.o -o $(TESTBUILDDIR)/p2p_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/transaction_test: $(OBJECTDIR)/transaction_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/transaction_test.o -o $(TESTBUILDDIR)/transaction_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/grpc_test: $(OBJECTDIR)/grpc_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/grpc_test.o -o $(TESTBUILDDIR)/grpc_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/memorydb_test: $(OBJECTDIR)/memorydb_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/memorydb_test.o -o $(TESTBUILDDIR)/memorydb_test  $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/overlaydb_test: $(OBJECTDIR)/overlaydb_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/overlaydb_test.o -o $(TESTBUILDDIR)/overlaydb_test  $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/statecachedb_test: $(OBJECTDIR)/statecachedb_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/statecachedb_test.o -o $(TESTBUILDDIR)/statecachedb_test  $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/trie_test: $(OBJECTDIR)/trie_test.o $(OBJECTDIR)/mem_trie.o  $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/trie_test.o $(OBJECTDIR)/mem_trie.o -o $(TESTBUILDDIR)/trie_test  $(LDFLAGS) $(LIBS)


protoc_taraxa_grpc: 
	@echo Refresh protobuf ...
	protoc -I. --grpc_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/taraxa_grpc.proto
	protoc -I. --cpp_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/taraxa_grpc.proto 

test: $(TESTBUILDDIR)/full_node_test $(TESTBUILDDIR)/dag_block_test $(TESTBUILDDIR)/network_test $(TESTBUILDDIR)/dag_test $(TESTBUILDDIR)/concur_hash_test $(TESTBUILDDIR)/transaction_test $(TESTBUILDDIR)/p2p_test $(TESTBUILDDIR)/grpc_test $(TESTBUILDDIR)/memorydb_test $(TESTBUILDDIR)/overlaydb_test $(TESTBUILDDIR)/statecachedb_test $(TESTBUILDDIR)/trie_test

run_test: test
	./$(TESTBUILDDIR)/memorydb_test
	./$(TESTBUILDDIR)/overlaydb_test
	./$(TESTBUILDDIR)/statecachedb_test
	./$(TESTBUILDDIR)/transaction_test
	./$(TESTBUILDDIR)/dag_test
	./$(TESTBUILDDIR)/concur_hash_test
	./$(TESTBUILDDIR)/dag_block_test
	./$(TESTBUILDDIR)/grpc_test
	./$(TESTBUILDDIR)/full_node_test
	./$(TESTBUILDDIR)/p2p_test
	./$(TESTBUILDDIR)/network_test
	./$(TESTBUILDDIR)/trie_test

ct:
	rm -rf $(TESTBUILDDIR)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILDIR) $(TESTBUILDDIR) $(OBJECTDIR)

.PHONY: run_test protoc grpc

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc

