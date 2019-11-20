include Makefile.common
include Makefile.aleth

main: $(BUILDDIR)/main
	@echo MAIN

all:  main

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
	${OBJECTDIR}/account_state/state.o \
	${OBJECTDIR}/account_state/state_snapshot.o \
	${OBJECTDIR}/account_state/state_registry.o \
	${OBJECTDIR}/genesis_state.o \
	${OBJECTDIR}/pbft_chain.o \
	${OBJECTDIR}/taraxa_capability.o \
	${OBJECTDIR}/sortition.o \
	${OBJECTDIR}/pbft_manager.o \
	${OBJECTDIR}/vote.o \
	${OBJECTDIR}/top.o \
	${OBJECTDIR}/config.o \
	${OBJECTDIR}/trx_engine/types.o \
	${OBJECTDIR}/trx_engine/trx_engine.o \
	${OBJECTDIR}/pbft_sortition_account.o \
	${OBJECTDIR}/database_face_cache.o \
	${OBJECTDIR}/replay_protection/sender_state.o \
	${OBJECTDIR}/replay_protection/replay_protection_service.o

NODE_OBJECTS := $(SUBMODULE_BUILT_INDICATORS) $(OBJECTFILES) $(ALETH_OBJ)

TESTS = \
	$(TESTBUILDDIR)/full_node_test \
	$(TESTBUILDDIR)/dag_block_test \
	$(TESTBUILDDIR)/network_test \
	$(TESTBUILDDIR)/dag_test \
	$(TESTBUILDDIR)/transaction_test \
	$(TESTBUILDDIR)/p2p_test \
	$(TESTBUILDDIR)/crypto_test \
    $(TESTBUILDDIR)/pbft_chain_test \
	$(TESTBUILDDIR)/pbft_rpc_test \
	$(TESTBUILDDIR)/pbft_manager_test

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

${OBJECTDIR}/database_face_cache.o: database_face_cache.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/database_face_cache.o database_face_cache.cpp $(CPPFLAGS)	

${OBJECTDIR}/executor.o: executor.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/executor.o executor.cpp $(CPPFLAGS)

${OBJECTDIR}/transaction_order_manager.o: transaction_order_manager.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/transaction_order_manager.o transaction_order_manager.cpp $(CPPFLAGS)

${OBJECTDIR}/account_state/state.o: account_state/state.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/account_state
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/account_state/state.o account_state/state.cpp $(CPPFLAGS)

${OBJECTDIR}/account_state/state_snapshot.o: account_state/state_snapshot.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/account_state
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/account_state/state_snapshot.o account_state/state_snapshot.cpp $(CPPFLAGS)

${OBJECTDIR}/account_state/state_registry.o: account_state/state_registry.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/account_state
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/account_state/state_registry.o account_state/state_registry.cpp $(CPPFLAGS)

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

${OBJECTDIR}/pbft_sortition_account.o: pbft_sortition_account.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/pbft_sortition_account.o pbft_sortition_account.cpp $(CPPFLAGS)

${OBJECTDIR}/replay_protection/sender_state.o: replay_protection/sender_state.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/replay_protection
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/replay_protection/sender_state.o replay_protection/sender_state.cpp $(CPPFLAGS)

${OBJECTDIR}/replay_protection/replay_protection_service.o: replay_protection/replay_protection_service.cpp
	${MKDIR} -p ${OBJECTDIR}
	${MKDIR} -p ${OBJECTDIR}/replay_protection
	${RM} "$@.d"
	${COMPILE} ${CXXFLAGS} "$@.d" -o ${OBJECTDIR}/replay_protection/replay_protection_service.o replay_protection/replay_protection_service.cpp $(CPPFLAGS)

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

node_objects: $(NODE_OBJECTS)

$(BUILDDIR)/main: $(NODE_OBJECTS) $(OBJECTDIR)/main.o
	${MKDIR} -p ${BUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/main.o -o $(BUILDDIR)/main $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/dag_test: $(OBJECTDIR)/dag_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/dag_test.o -o $(TESTBUILDDIR)/dag_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/network_test: $(OBJECTDIR)/network_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/network_test.o -o $(TESTBUILDDIR)/network_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/dag_block_test: $(OBJECTDIR)/dag_block_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/dag_block_test.o -o $(TESTBUILDDIR)/dag_block_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/full_node_test: $(OBJECTDIR)/full_node_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/full_node_test.o -o $(TESTBUILDDIR)/full_node_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/p2p_test: $(OBJECTDIR)/p2p_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/p2p_test.o -o $(TESTBUILDDIR)/p2p_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/transaction_test: $(OBJECTDIR)/transaction_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}	
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/transaction_test.o -o $(TESTBUILDDIR)/transaction_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS) 

$(TESTBUILDDIR)/crypto_test: $(OBJECTDIR)/crypto_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/crypto_test.o -o $(TESTBUILDDIR)/crypto_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/pbft_chain_test: $(OBJECTDIR)/pbft_chain_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/pbft_chain_test.o -o $(TESTBUILDDIR)/pbft_chain_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/pbft_rpc_test: $(OBJECTDIR)/pbft_rpc_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/pbft_rpc_test.o -o $(TESTBUILDDIR)/pbft_rpc_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/pbft_manager_test: $(OBJECTDIR)/pbft_manager_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/pbft_manager_test.o -o $(TESTBUILDDIR)/pbft_manager_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/performance_test: $(OBJECTDIR)/performance_test.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/performance_test.o -o $(TESTBUILDDIR)/performance_test  $(LDFLAGS) $(LIBS) $(CPPFLAGS)

$(TESTBUILDDIR)/prometheus_demo: $(OBJECTDIR)/prometheus_demo.o $(NODE_OBJECTS)
	${MKDIR} -p ${TESTBUILDDIR}
	$(CXX) -std=c++17 $(OBJECTFILES) $(ALETH_OBJ) $(OBJECTDIR)/prometheus_demo.o -o $(TESTBUILDDIR)/prometheus_demo $(LDFLAGS) $(LIBS) $(CPPFLAGS)

test: $(TESTS)

perf_test: $(TESTBUILDDIR)/performance_test

run_perf_test: perf_test
	./$(TESTBUILDDIR)/performance_test

run_test: test
	./scripts/run_commands_long_circuit.sh $(TESTS)

pdemo: ${OBJECTDIR}/prometheus_demo.o $(TESTBUILDDIR)/prometheus_demo main
	./$(TESTBUILDDIR)/prometheus_demo $(PUSHGATEWAY_IP) $(PUSHGATEWAY_PORT) $(PUSHGATEWAY_NAME)

ct:
	rm -rf $(TESTBUILDDIR)

c: clean
clean:
	@echo CLEAN && rm -rf $(BUILDDIR) $(TESTBUILDDIR) $(OBJECTDIR)

.PHONY: run_test
