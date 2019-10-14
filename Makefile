DEPENDENCIES = submodules/cryptopp/libcryptopp.a \
	submodules/ethash/build/lib/ethash/libethash.a \
	submodules/libff/build/libff/libff.a \
	submodules/secp256k1/.libs/libsecp256k1.a \
	submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-core.a \
	submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-pull.a \
	submodules/prometheus-cpp/_build/deploy/usr/local/lib/libprometheus-cpp-push.a \
	trx_engine/trx_engine.a
ifneq (,$(shell git submodule update --recursive --init))
    $(info Cleaning up built dependencies cause the submodules changed: $(DEPENDENCIES))
    $(shell rm -rf $(DEPENDENCIES))
endif
# adjust these to your system by calling e.g. make CXX=asdf LIBS=qwerty
CXX := g++
CPPFLAGS := -I submodules -I submodules/rapidjson/include -I submodules/libff -I submodules/libff/libff -I submodules/ethash/include -I . -I submodules/prometheus-cpp/push/include -I submodules/prometheus-cpp/pull/include -I submodules/prometheus-cpp/core/include -I submodules/secp256k1/include -I/usr/include/jsoncpp -I submodules/taraxa-evm -DBOOST_LOG_DYN_LINK -DETH_FATDB
OS := $(shell uname)
LOG_LIB = -lboost_log-mt
ifneq ($(OS), Darwin) #Mac
	CPPFLAGS += -DCRYPTOPP_DISABLE_ASM 
	LOG_LIB = -lboost_log
endif
DEBUG = 0
PERF = 0
CXXFLAGS := -std=c++17 -c -O3 -MMD -MP -MF 
CXXFLAGS2 := -std=c++17 -c -O3 -MMD -MP -MF 
LIBS := -DBOOST_LOG_DYN_LINK $(LOG_LIB) -lleveldb -lrocksdb -lsecp256k1 -lgmp -lscrypt -lpthread -lboost_program_options -lboost_filesystem -lboost_system -lboost_log_setup -lboost_log -lcryptopp -lethash -lff -lgtest -lboost_thread-mt -lrocksdb -lprometheus-cpp-core -lprometheus-cpp-push -lprometheus-cpp-pull -lz -lcurl -ljsoncpp -ljsonrpccpp-common -ljsonrpccpp-server trx_engine/trx_engine.a
ifeq ($(OS), Darwin)
	LIBS += -framework CoreFoundation -framework Security
endif
OBJECTDIR := obj
BUILDDIR := build
TESTBUILDDIR := test_build

# Note: makefile translates `$$?` into `$?`
LIBATOMIC_NOT_FOUND = $(shell \
    $(CXX) $(LDFLAGS) -latomic -shared -o /dev/null &> /dev/null; echo $$? \
)
# Optional linking for libatomic (part of standard library).
# Some toolchains provide this library,
# and assume programs using <atomic> would link against it.
ifeq ($(LIBATOMIC_NOT_FOUND), 0)
    LIBS += -latomic
endif

# Note: gperftools seems not work in Darwin
ifneq ($(PERF), 0)
 	CPPFLAGS += -fno-omit-frame-pointer 
		LIBS += -lprofiler -ltcmalloc
endif

ifneq ($(DEBUG), 0)
	CXXFLAGS := -std=c++17 -c -g -MMD -MP -MF 
	CXXFLAGS2 := -std=c++17 -c -g -MMD -MP -MF
	CPPFLAGS += -Wl,--export-dynamic 
	BUILDDIR := build-d
	TESTBUILDDIR := test_build-d
	OBJECTDIR := obj-d
endif
LDFLAGS := -L submodules/cryptopp -L submodules/ethash/build/lib/ethash -L submodules/libff/build/libff -L submodules/secp256k1/.libs -L submodules/prometheus-cpp/_build/deploy/usr/local/lib
MKDIR := mkdir
RM := rm -f

COMPILE = $(CXX) $(CXXFLAGS)

.depcheck-impl:
	@echo "DEPFILES=\$$(wildcard \$$(addsuffix .d, \$${OBJECTFILES} ))" >.dep.inc; \
	echo "DEPFILES+=\$$(wildcard \$$(addsuffix .d, \$${P2POBJECTFILES} ))" >>.dep.inc; \
	echo "DEPFILES+=\$$(wildcard \$$(addsuffix .d, \$${MAINOBJECTFILES} ))" >>.dep.inc; \
	echo "ifneq (\$${DEPFILES},)" >>.dep.inc; \
	echo "include \$${DEPFILES}" >>.dep.inc; \
	echo "endif" >>.dep.inc; \

main: $(BUILDDIR)/main
	@echo MAIN

all:  $(DEPENDENCIES) main

include p2p.inc


OBJECTFILES= \
	${OBJECTDIR}/dag_block.o \
	${OBJECTDIR}/util.o \
	${OBJECTDIR}/network.o \
	${OBJECTDIR}/full_node.o \
	${OBJECTDIR}/types.o \
	${OBJECTDIR}/dag.o \
	${OBJECTDIR}/block_proposer.o \
	${OBJECTDIR}/transaction.o \
	${OBJECTDIR}/executor.o \
	${OBJECTDIR}/transaction_order_manager.o \
	${OBJECTDIR}/state_registry.o \
	${OBJECTDIR}/genesis_state.o \
	${OBJECTDIR}/pbft_chain.o \
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
	${OBJECTDIR}/simple_overlaydb_delegate.o \
	${OBJECTDIR}/trx_engine/types.o \
	${OBJECTDIR}/trx_engine/trx_engine.o

MAINOBJECTFILES= \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/p2p_test.o \
	${OBJECTDIR}/dag_test.o \
	${OBJECTDIR}/network_test.o \
	${OBJECTDIR}/dag_block_test.o \
	${OBJECTDIR}/full_node_test.o \
	${OBJECTDIR}/pbft_chain_test.o \
	${OBJECTDIR}/transaction_test.o \
	${OBJECTDIR}/crypto_test.o \
	${OBJECTDIR}/pbft_rpc_test.o \
	${OBJECTDIR}/pbft_manager_test.o

ifeq ($(PERF), 1)
	MAINOBJECTFILES += ${OBJECTDIR}/performance_test.o
endif

${OBJECTDIR}/config.o: config.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/config.o config.cpp $(CPPFLAGS)

${OBJECTDIR}/top.o: top.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/top.o top.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_chain.o: pbft_chain.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_chain.o pbft_chain.cpp $(CPPFLAGS)
	
${OBJECTDIR}/executor.o: executor.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/executor.o executor.cpp $(CPPFLAGS)

${OBJECTDIR}/transaction_order_manager.o: transaction_order_manager.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/transaction_order_manager.o transaction_order_manager.cpp $(CPPFLAGS)

${OBJECTDIR}/state_registry.o: state_registry.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/state_registry.o state_registry.cpp $(CPPFLAGS)

${OBJECTDIR}/genesis_state.o: genesis_state.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/genesis_state.o genesis_state.cpp $(CPPFLAGS)

${OBJECTDIR}/dag_block.o: dag_block.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/dag_block.o dag_block.cpp $(CPPFLAGS)
	
${OBJECTDIR}/util.o: util.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/util.o util.cpp $(CPPFLAGS)

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
	
${OBJECTDIR}/dag.o: dag.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/dag.o dag.cpp $(CPPFLAGS)
	
${OBJECTDIR}/block_proposer.o: block_proposer.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/block_proposer.o block_proposer.cpp $(CPPFLAGS)

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

${OBJECTDIR}/simple_overlaydb_delegate.o: simple_overlaydb_delegate.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/simple_overlaydb_delegate.o simple_overlaydb_delegate.cpp $(CPPFLAGS)

${OBJECTDIR}/trx_engine/types.o: trx_engine/types.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/trx_engine
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/trx_engine/types.o trx_engine/types.cpp $(CPPFLAGS)

${OBJECTDIR}/trx_engine/trx_engine.o: trx_engine/trx_engine.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/trx_engine
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/trx_engine/trx_engine.o trx_engine/trx_engine.cpp $(CPPFLAGS)

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

${OBJECTDIR}/crypto_test.o: core_tests/crypto_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/crypto_test.o core_tests/crypto_test.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_rpc_test.o: core_tests/pbft_rpc_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_rpc_test.o core_tests/pbft_rpc_test.cpp $(CPPFLAGS)

${OBJECTDIR}/pbft_manager_test.o: core_tests/pbft_manager_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_manager_test.o core_tests/pbft_manager_test.cpp $(CPPFLAGS)

${OBJECTDIR}/performance_test.o: core_tests/performance_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/performance_test.o core_tests/performance_test.cpp $(CPPFLAGS)

${OBJECTDIR}/prometheus_demo.o: prometheus_demo.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/prometheus_demo.o prometheus_demo.cpp $(CPPFLAGS)

trx_engine/trx_engine.a:
	@echo Building Go trx engine static C library
	cd submodules/taraxa-evm; go build -tags=secp256k1_no_cgo -buildmode=c-archive -o ../../trx_engine/trx_engine.a

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

$(BUILDDIR)/main: $(DEPENDENCIES) $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/main.o
	${MKDIR} -p ${BUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/main.o -o $(BUILDDIR)/main $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/dag_test: $(OBJECTDIR)/dag_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/dag_test.o -o $(TESTBUILDDIR)/dag_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/network_test: $(OBJECTDIR)/network_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/network_test.o -o $(TESTBUILDDIR)/network_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/dag_block_test: $(OBJECTDIR)/dag_block_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/dag_block_test.o -o $(TESTBUILDDIR)/dag_block_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/full_node_test: $(OBJECTDIR)/full_node_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/full_node_test.o -o $(TESTBUILDDIR)/full_node_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/p2p_test: $(OBJECTDIR)/p2p_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/p2p_test.o -o $(TESTBUILDDIR)/p2p_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/transaction_test: $(OBJECTDIR)/transaction_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/transaction_test.o -o $(TESTBUILDDIR)/transaction_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/crypto_test: $(OBJECTDIR)/crypto_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/crypto_test.o -o $(TESTBUILDDIR)/crypto_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/pbft_chain_test: $(OBJECTDIR)/pbft_chain_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/pbft_chain_test.o -o $(TESTBUILDDIR)/pbft_chain_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/pbft_rpc_test: $(OBJECTDIR)/pbft_rpc_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/pbft_rpc_test.o -o $(TESTBUILDDIR)/pbft_rpc_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/pbft_manager_test: $(OBJECTDIR)/pbft_manager_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/pbft_manager_test.o -o $(TESTBUILDDIR)/pbft_manager_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/performance_test: $(OBJECTDIR)/performance_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/performance_test.o -o $(TESTBUILDDIR)/performance_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/prometheus_demo: $(OBJECTDIR)/prometheus_demo.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/prometheus_demo.o -o $(TESTBUILDDIR)/prometheus_demo $(LDFLAGS) $(LIBS) $(CPPFLAGS)

test: $(TESTBUILDDIR)/full_node_test $(TESTBUILDDIR)/dag_block_test $(TESTBUILDDIR)/network_test $(TESTBUILDDIR)/dag_test $(TESTBUILDDIR)/transaction_test $(TESTBUILDDIR)/p2p_test $(TESTBUILDDIR)/crypto_test $(TESTBUILDDIR)/pbft_chain_test $(TESTBUILDDIR)/pbft_rpc_test $(TESTBUILDDIR)/pbft_manager_test

perf_test: $(TESTBUILDDIR)/performance_test

run_perf_test: perf_test
	./$(TESTBUILDDIR)/performance_test

TESTBUILDDIR=build
run_test:
	exit_code_sum=0;\
	./$(TESTBUILDDIR)/crypto_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/pbft_rpc_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/transaction_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/dag_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/dag_block_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/full_node_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/p2p_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/network_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/pbft_chain_test;\
	(( exit_code_sum += $$? ));\
	./$(TESTBUILDDIR)/pbft_manager_test;\
	(( exit_code_sum += $$? ));\
	test $$exit_code_sum -eq 0

pdemo: ${OBJECTDIR}/prometheus_demo.o $(TESTBUILDDIR)/prometheus_demo main
	./$(TESTBUILDDIR)/prometheus_demo $(PUSHGATEWAY_IP) $(PUSHGATEWAY_PORT) $(PUSHGATEWAY_NAME)

ct:
	rm -rf $(TESTBUILDDIR)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILDDIR) $(TESTBUILDDIR) $(OBJECTDIR)

.PHONY: run_test protoc 

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc

