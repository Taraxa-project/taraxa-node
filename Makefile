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
LIBS := -DBOOST_LOG_DYN_LINK $(LOG_LIB) -lleveldb -lrocksdb -lsecp256k1 -lgmp -lscrypt -lpthread -lboost_program_options -lboost_filesystem -lboost_system -lcryptopp -lethash -lff -lgtest -lboost_thread-mt -lrocksdb
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

include p2p.inc

OBJECTFILES= \
	${OBJECTDIR}/rocks_db.o \
	${OBJECTDIR}/state_block.o \
	${OBJECTDIR}/util.o \
	${OBJECTDIR}/udp_buffer.o \
	${OBJECTDIR}/network.o \
	${OBJECTDIR}/full_node.o \
	${OBJECTDIR}/types.o \
	${OBJECTDIR}/visitor.o \
	${OBJECTDIR}/dag.o \
	${OBJECTDIR}/block_proposer.o \
	${OBJECTDIR}/rpc.o

MAINOBJECTFILES= \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/p2p_test.o \
	${OBJECTDIR}/dag_test.o \
	${OBJECTDIR}/network_test.o \
	${OBJECTDIR}/state_block_test.o \
	${OBJECTDIR}/full_node_test.o \
	${OBJECTDIR}/concur_hash_test.o

	
${OBJECTDIR}/rocks_db.o: rocks_db.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/rocks_db.o rocks_db.cpp $(CPPFLAGS)
	
${OBJECTDIR}/state_block.o: state_block.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/state_block.o state_block.cpp $(CPPFLAGS)
	
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

${OBJECTDIR}/state_block_test.o: core_tests/state_block_test.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/state_block_test.o core_tests/state_block_test.cpp $(CPPFLAGS)	

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

${OBJECTDIR}/concur_hash.o: concur_storage/concur_hash.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/concur_hash.o concur_storage/concur_hash.cpp $(CPPFLAGS)

${OBJECTDIR}/conflict_detector.o: concur_storage/conflict_detector.cpp 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/conflict_detector.o concur_storage/conflict_detector.cpp $(CPPFLAGS)

TESTS = \
    core_tests/dag_test.cpp

all: $(DEPENDENCIES) main

DEPENDENCIES = submodules/cryptopp/libcryptopp.a \
	submodules/ethash/build/lib/ethash/libethash.a \
	submodules/libff/build/libff/libff.a \
	submodules/secp256k1/.libs/libsecp256k1.a
		

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
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/main.o -o $(BUILDDIR)/main $(LDFLAGS) $(LIBS)


$(TESTBUILDDIR)/dag_test: $(OBJECTDIR)/dag_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/dag_test.o -o $(TESTBUILDDIR)/dag_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/network_test: $(OBJECTDIR)/network_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/network_test.o -o $(TESTBUILDDIR)/network_test  $(LDFLAGS) $(LIBS) 
	
$(TESTBUILDDIR)/state_block_test: $(OBJECTDIR)/state_block_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/state_block_test.o -o $(TESTBUILDDIR)/state_block_test  $(LDFLAGS) $(LIBS) 
	
$(TESTBUILDDIR)/full_node_test: $(OBJECTDIR)/full_node_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/full_node_test.o -o $(TESTBUILDDIR)/full_node_test  $(LDFLAGS) $(LIBS) 
	
$(TESTBUILDDIR)/concur_hash_test: $(OBJECTDIR)/concur_hash_test.o ${OBJECTDIR}/concur_hash.o ${OBJECTDIR}/conflict_detector.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/concur_hash_test.o ${OBJECTDIR}/concur_hash.o ${OBJECTDIR}/conflict_detector.o -o $(TESTBUILDDIR)/concur_hash_test  $(LDFLAGS) $(LIBS) 
	
$(TESTBUILDDIR)/p2p_test: $(OBJECTDIR)/p2p_test.o $(OBJECTFILES) $(P2POBJECTFILES) $(DEPENDENCIES)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(P2POBJECTFILES) $(OBJECTDIR)/p2p_test.o -o $(TESTBUILDDIR)/p2p_test  $(LDFLAGS) $(LIBS) 

$(TESTBUILDDIR)/transaction_test: create_test_dir
	g++ -std=c++17 $(GOOGLE_APIS_FLAG) -o $(TESTBUILDDIR)/transaction_test core_tests/transaction_test.cpp transaction.cpp types.cpp util.cpp grpc_server.cpp grpc_util.cpp grpc/proto/taraxa_grpc.pb.cc grpc/proto/taraxa_grpc.grpc.pb.cc $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -lboost_system

$(TESTBUILDDIR)/grpc_test: create_test_dir protoc_taraxa_grpc
	g++ -std=c++17 $(GOOGLE_APIS_FLAG) -o $(TESTBUILDDIR)/grpc_test core_tests/grpc_test.cpp grpc_client.cpp grpc_server.cpp grpc_util.cpp transaction.cpp types.cpp util.cpp grpc/proto/taraxa_grpc.pb.cc grpc/proto/taraxa_grpc.grpc.pb.cc $(CPPFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lboost_thread-mt -lboost_system
	
protoc: 
	protoc -I. --grpc_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/ledger.proto
	protoc -I. --cpp_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/ledger.proto 

main: $(BUILDDIR)/main
	@echo MAIN

protoc_taraxa_grpc: 
	protoc -I. --grpc_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/taraxa_grpc.proto
	protoc -I. --cpp_out=./grpc --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin proto/taraxa_grpc.proto 


main: $(BUILDDIR)/main
	@echo MAIN

test: $(TESTBUILDDIR)/full_node_test $(TESTBUILDDIR)/state_block_test $(TESTBUILDDIR)/network_test $(TESTBUILDDIR)/dag_test $(TESTBUILDDIR)/concur_hash_test $(TESTBUILDDIR)/p2p_test

run_test: test
	./$(TESTBUILDDIR)/dag_test
	./$(TESTBUILDDIR)/network_test
	./$(TESTBUILDDIR)/state_block_test
	./$(TESTBUILDDIR)/full_node_test
	./$(TESTBUILDDIR)/concur_hash_test
	./$(TESTBUILDDIR)/p2p_test
	./$(TEST_BUILD)/transaction_test
#	./$(TEST_BUILD)/grpc_test


ct:
	rm -rf $(TESTBUILDDIR)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILDIR) $(TESTBUILDDIR) $(OBJECTDIR)

.PHONY: run_test protoc grpc

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc

