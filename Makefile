# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -I submodules -I submodules/rapidjson/include -I submodules/libff -I submodules/libff/libff -I submodules/ethash/include -I . -I concur_storage -I grpc -I submodules/prometheus-cpp/push/include -I submodules/prometheus-cpp/pull/include -I submodules/prometheus-cpp/core/include -I submodules/secp256k1/include -I/usr/include/jsoncpp -DBOOST_LOG_DYN_LINK -DETH_FATDB
OS := $(shell uname)
LOG_LIB = -lboost_log-mt
ifneq ($(OS), Darwin) #Mac
	CPPFLAGS += -DCRYPTOPP_DISABLE_ASM 
	LOG_LIB = -lboost_log
endif
DEBUG = 0
CXXFLAGS := -std=c++17 -c -O3 -MMD -MP -MF 
CXXFLAGS2 := -std=c++17 -c -O3 -MMD -MP -MF 
OBJECTDIR := obj
BUILDDIR := build
TESTBUILDDIR := test_build
ifneq ($(DEBUG), 0)
	CXXFLAGS := -std=c++17 -c -g -MMD -MP -MF 
	CXXFLAGS2 := -std=c++17 -c -g -MMD -MP -MF 
	CPPFLAGS += -Wl,--export-dynamic
	BUILDDIR := build-d
	TESTBUILDDIR := test_build-d
	OBJECTDIR := obj-d
endif
LDFLAGS := -L submodules/cryptopp -L submodules/ethash/build/lib/ethash -L submodules/libff/build/libff -L submodules/secp256k1/.libs -L submodules/prometheus-cpp/_build/deploy/usr/local/lib
LIBS := -DBOOST_LOG_DYN_LINK $(LOG_LIB) -lleveldb -lrocksdb -lsecp256k1 -lgmp -lscrypt -lpthread -lboost_program_options -lboost_filesystem -lboost_system -lboost_log_setup -lboost_log -lcryptopp -lethash -lff -lgtest -lboost_thread-mt -lrocksdb -lprometheus-cpp-core -lprometheus-cpp-push -lprometheus-cpp-pull -lz -lcurl -ljsoncpp -ljsonrpccpp-common -ljsonrpccpp-server
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
	${OBJECTDIR}/libethereum/account.o \
	${OBJECTDIR}/libethcore/log_entry.o \
	${OBJECTDIR}/libethereum/transaction_receipt.o \
	${OBJECTDIR}/libethereum/state.o \
	${OBJECTDIR}/sortition.o \
	${OBJECTDIR}/pbft_manager.o \
	${OBJECTDIR}/vote.o \
	${OBJECTDIR}/top.o \
	${OBJECTDIR}/config.o \
	${OBJECTDIR}/SimpleStateDBDelegate.o \
	${OBJECTDIR}/SimpleTaraxaRocksDBDelegate.o \
	${OBJECTDIR}/SimpleOverlayDBDelegate.o

MAINOBJECTFILES= \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/p2p_test.o \
	${OBJECTDIR}/dag_test.o \
	${OBJECTDIR}/network_test.o \
	${OBJECTDIR}/dag_block_test.o \
	${OBJECTDIR}/full_node_test.o \
	${OBJECTDIR}/pbft_chain_test.o \
	${OBJECTDIR}/concur_hash_test.o \
	${OBJECTDIR}/transaction_test.o \
	${OBJECTDIR}/grpc_test.o \
	${OBJECTDIR}/memorydb_test.o \
	${OBJECTDIR}/overlaydb_test.o \
	${OBJECTDIR}/statecachedb_test.o \
	${OBJECTDIR}/trie_test.o \
	${OBJECTDIR}/crypto_test.o \
	${OBJECTDIR}/state_unit_tests.o \
	${OBJECTDIR}/pbft_rpc_test.o \
	${OBJECTDIR}/pbft_manager_test.o

${OBJECTDIR}/taraxa_grpc.pb.o: grpc/proto/taraxa_grpc.pb.cc
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/taraxa_grpc.pb.o grpc/proto/taraxa_grpc.pb.cc $(CPPFLAGS)

${OBJECTDIR}/taraxa_grpc.grpc.pb.o: grpc/proto/taraxa_grpc.grpc.pb.cc
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/taraxa_grpc.grpc.pb.o grpc/proto/taraxa_grpc.grpc.pb.cc $(CPPFLAGS)

${OBJECTDIR}/config.o: config.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/config.o config.cpp $(CPPFLAGS)

${OBJECTDIR}/top.o: top.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/top.o top.cpp $(CPPFLAGS)

${OBJECTDIR}/rocks_db.o: rocks_db.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/rocks_db.o rocks_db.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_chain.o: pbft_chain.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_chain.o pbft_chain.cpp $(CPPFLAGS)
	
${OBJECTDIR}/executor.o: executor.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/executor.o executor.cpp $(CPPFLAGS)
	
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

${OBJECTDIR}/libethereum/account.o: libethereum/Account.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/libethereum
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/libethereum/account.o libethereum/Account.cpp $(CPPFLAGS)

# required for TransactionReceipt
${OBJECTDIR}/libethcore/log_entry.o: libethcore/LogEntry.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/libethcore
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/libethcore/log_entry.o libethcore/LogEntry.cpp $(CPPFLAGS)

${OBJECTDIR}/libethereum/transaction_receipt.o: libethereum/TransactionReceipt.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/libethereum
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/libethereum/transaction_receipt.o libethereum/TransactionReceipt.cpp $(CPPFLAGS)

${OBJECTDIR}/libethereum/state.o: libethereum/State.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/libethereum
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/libethereum/state.o libethereum/State.cpp $(CPPFLAGS)

${OBJECTDIR}/taraxa_capability.o: taraxa_capability.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/taraxa_capability.o taraxa_capability.cpp $(CPPFLAGS)

${OBJECTDIR}/sortition.o: sortition.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/sortition.o sortition.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_manager.o: pbft_manager.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_manager.o pbft_manager.cpp $(CPPFLAGS)

${OBJECTDIR}/vote.o: vote.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/vote.o vote.cpp $(CPPFLAGS)

${OBJECTDIR}/SimpleOverlayDBDelegate.o: SimpleOverlayDBDelegate.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/SimpleOverlayDBDelegate.o SimpleOverlayDBDelegate.cpp $(CPPFLAGS)

${OBJECTDIR}/SimpleTaraxaRocksDBDelegate.o: SimpleTaraxaRocksDBDelegate.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/SimpleTaraxaRocksDBDelegate.o SimpleTaraxaRocksDBDelegate.cpp $(CPPFLAGS)

${OBJECTDIR}/SimpleStateDBDelegate.o: SimpleStateDBDelegate.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/SimpleStateDBDelegate.o SimpleStateDBDelegate.cpp $(CPPFLAGS)

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

${OBJECTDIR}/long_network_test.o: core_tests/long_network_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/long_network_test.o core_tests/long_network_test.cpp $(CPPFLAGS)

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

${OBJECTDIR}/pbft_chain_test.o: core_tests/pbft_chain_test.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_chain_test.o core_tests/pbft_chain_test.cpp $(CPPFLAGS)

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

${OBJECTDIR}/crypto_test.o: core_tests/crypto_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/crypto_test.o core_tests/crypto_test.cpp $(CPPFLAGS)

${OBJECTDIR}/state_unit_tests.o: core_tests/state_unit_tests.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/state_unit_tests.o core_tests/state_unit_tests.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_rpc_test.o: core_tests/pbft_rpc_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_rpc_test.o core_tests/pbft_rpc_test.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_manager_test.o: core_tests/pbft_manager_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_manager_test.o core_tests/pbft_manager_test.cpp $(CPPFLAGS)

${OBJECTDIR}/prometheus_demo.o: prometheus_demo.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/prometheus_demo.o prometheus_demo.cpp $(CPPFLAGS)

DEPENDENCIES = submodules/cryptopp/libcryptopp.a \
	submodules/ethash/build/lib/ethash/libethash.a \
	submodules/libff/build/libff/libff.a \
	submodules/secp256k1/.libs/libsecp256k1.a \
	core_tests/create_samples.hpp \
	submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-core.a \
	submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-pull.a \
	submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-push.a

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

submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-core.a submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-pull.a submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-push.a:
	@echo Attempting to compile libprometheus, if it fails try compiling it manually. See https://github.com/jupp0r/prometheus-cpp
	cd submodules/prometheus-cpp; git submodule update --init 3rdparty/civetweb/; mkdir -p _build; cd _build; cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF; make -j 4; mkdir -p deploy; make DESTDIR=`pwd`/deploy install

$(BUILDDIR)/main: $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES) $(OBJECTDIR)/main.o
	${MKDIR} -p ${BUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/main.o -o $(BUILDDIR)/main $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/dag_test: $(OBJECTDIR)/dag_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/dag_test.o -o $(TESTBUILDDIR)/dag_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/network_test: $(OBJECTDIR)/network_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/network_test.o -o $(TESTBUILDDIR)/network_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/long_network_test: $(OBJECTDIR)/long_network_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/long_network_test.o -o $(TESTBUILDDIR)/long_network_test  $(LDFLAGS) $(LIBS) 

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

$(TESTBUILDDIR)/crypto_test: $(OBJECTDIR)/crypto_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/crypto_test.o -o $(TESTBUILDDIR)/crypto_test  $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/pbft_chain_test: $(OBJECTDIR)/pbft_chain_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/pbft_chain_test.o -o $(TESTBUILDDIR)/pbft_chain_test  $(LDFLAGS) $(LIBS)
$(TESTBUILDDIR)/state_unit_tests: $(OBJECTDIR)/state_unit_tests.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/state_unit_tests.o -o $(TESTBUILDDIR)/state_unit_tests  $(LDFLAGS) $(LIBS)


$(TESTBUILDDIR)/pbft_rpc_test: $(OBJECTDIR)/pbft_rpc_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/pbft_rpc_test.o -o $(TESTBUILDDIR)/pbft_rpc_test  $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/pbft_manager_test: $(OBJECTDIR)/pbft_manager_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/pbft_manager_test.o -o $(TESTBUILDDIR)/pbft_manager_test  $(LDFLAGS) $(LIBS)

$(TESTBUILDDIR)/prometheus_demo: $(OBJECTDIR)/prometheus_demo.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(GOOGLE_APIS_FLAG) $(P2POBJECTFILES) $(OBJECTDIR)/prometheus_demo.o -o $(TESTBUILDDIR)/prometheus_demo $(LDFLAGS) $(LIBS)

protoc_taraxa_grpc: 
	@echo Refresh protobuf ...
	protoc -I. --grpc_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/taraxa_grpc.proto
	protoc -I. --cpp_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/taraxa_grpc.proto 

test: $(TESTBUILDDIR)/full_node_test $(TESTBUILDDIR)/dag_block_test $(TESTBUILDDIR)/network_test $(TESTBUILDDIR)/dag_test $(TESTBUILDDIR)/concur_hash_test $(TESTBUILDDIR)/transaction_test $(TESTBUILDDIR)/p2p_test $(TESTBUILDDIR)/grpc_test $(TESTBUILDDIR)/memorydb_test $(TESTBUILDDIR)/overlaydb_test $(TESTBUILDDIR)/statecachedb_test $(TESTBUILDDIR)/trie_test $(TESTBUILDDIR)/crypto_test $(TESTBUILDDIR)/pbft_chain_test $(TESTBUILDDIR)/state_unit_tests $(TESTBUILDDIR)/pbft_rpc_test $(TESTBUILDDIR)/pbft_manager_test

run_test: test main
	./$(TESTBUILDDIR)/crypto_test
	./$(TESTBUILDDIR)/pbft_rpc_test
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
	./$(TESTBUILDDIR)/pbft_chain_test
	./$(TESTBUILDDIR)/state_unit_tests
	./$(TESTBUILDDIR)/pbft_manager_test
pdemo: ${OBJECTDIR}/prometheus_demo.o $(TESTBUILDDIR)/prometheus_demo main
	./$(TESTBUILDDIR)/prometheus_demo $(PUSHGATEWAY_IP) $(PUSHGATEWAY_PORT) $(PUSHGATEWAY_NAME)

ct:
	rm -rf $(TESTBUILDDIR)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILDIR) $(TESTBUILDDIR) $(OBJECTDIR)

.PHONY: run_test protoc grpc

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc

