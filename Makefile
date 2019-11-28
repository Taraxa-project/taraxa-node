include Makefile.common
include Makefile.submodules

include $(shell find $(BUILD_DIR) -path "*.d")

DEPS := $(SUBMODULE_DEPS)

ifneq ($(DEBUG), 0)
	COMPILE_FLAGS := -g
else
	COMPILE_FLAGS := -O3
endif
ifneq ($(PERF), 0)
 	COMPILE_FLAGS += -fno-omit-frame-pointer
endif

COMPILE_DEFINITIONS := \
	$(BOOST_COMPILE_DEFINITIONS) \
	$(ALETH_COMPILE_DEFINITIONS) \
	$(CRYPTOPP_COMPILE_DEFIINITIONS)

INCLUDE_DIRS := $(CURDIR) $(DEPS_INSTALL_PREFIX)/include $(JSONCPP_INCLUDE_DIR)

LINK_FLAGS := -Wl,-rpath $(DEPS_INSTALL_PREFIX)/lib
ifneq ($(DEBUG), 0)
	ifeq ($(OS),Dawrin)
		LINK_FLAGS += -Wl,--export-dynamic
	else
		LINK_FLAGS += -rdynamic
	endif
endif

LIB_DIRS := $(DEPS_INSTALL_PREFIX)/lib

BOOST_LIBS := \
	boost_program_options \
	boost_filesystem \
	boost_system \
	boost_thread-mt \
	boost_log_setup
ifeq ($(OS), Darwin)
	BOOST_LIBS += boost_log-mt
else
	BOOST_LIBS += boost_log
endif
LIBS := \
	pthread \
	z \
	curl \
	ssl \
	crypto \
	gmp \
	gmpxx \
	mpfr \
	$(BOOST_LIBS) \
	leveldb \
	rocksdb \
	sodium \
	vdf \
	scrypt \
	prometheus-cpp-core \
	prometheus-cpp-push \
	prometheus-cpp-pull \
	jsoncpp \
	jsonrpccpp-common \
	jsonrpccpp-server \
	ff \
	secp256k1 \
	cryptopp \
	ethash \
	$(ALETH_LIBS) \
	$(TARAXA_EVM_LIB) \
	gtest
# Optional linking for libatomic (part of standard library).
# Some toolchains provide this library,
# and assume programs using <atomic> would link against it.
# Note: makefile translates `$$?` into `$?`
LIBATOMIC_NOT_FOUND = $(shell \
    $(CXX) $(LIB_DIRS) -latomic -shared -o /dev/null &> /dev/null; echo $$? \
)
ifeq ($(LIBATOMIC_NOT_FOUND), 0)
    LIBS += atomic
endif
ifeq ($(OS), Darwin)
	OSX_FRAMEWORKS := CoreFoundation Security
endif

NODE_SRCS := \
	util.cpp \
    dag_block.cpp \
    network.cpp \
    full_node.cpp \
    types.cpp \
    dag.cpp \
    block_proposer.cpp \
    transaction.cpp \
    executor.cpp \
    transaction_order_manager.cpp \
    pbft_chain.cpp \
    taraxa_capability.cpp \
    sortition.cpp \
    pbft_manager.cpp \
    vote.cpp \
    top.cpp \
    config.cpp \
    genesis_state.cpp \
    account_state/state.cpp \
    account_state/state_snapshot.cpp \
    account_state/state_registry.cpp \
    trx_engine/trx_engine.cpp \
    trx_engine/types.cpp \
    pbft_sortition_account.cpp \
    database_face_cache.cpp \
    replay_protection/sender_state.cpp \
    replay_protection/replay_protection_service.cpp \
    net/JsonHelper.cpp \
    net/RpcServer.cpp \
    net/WSServer.cpp \
    net/Test.cpp \
    net/Taraxa.cpp \
    net/Net.cpp
NODE_OBJS := $(addprefix $(OBJ_DIR)/, $(NODE_SRCS:.cpp=.o))

TEST_SRCS := \
	core_tests/full_node_test.cpp \
	core_tests/dag_block_test.cpp \
	core_tests/network_test.cpp \
	core_tests/dag_test.cpp \
	core_tests/transaction_test.cpp \
	core_tests/p2p_test.cpp \
	core_tests/crypto_test.cpp \
    core_tests/pbft_chain_test.cpp \
	core_tests/pbft_rpc_test.cpp \
	core_tests/pbft_manager_test.cpp
TEST_OBJS := $(addprefix $(OBJ_DIR)/, $(TEST_SRCS:.cpp=.o))
TESTS := $(addprefix $(BIN_DIR)/, $(basename $(TEST_SRCS)))


$(OBJ_DIR)/%.o: %.cpp $(DEPS)
	mkdir -p $(@D)
	$(strip \
		$(CXX) -c -std=$(CXX_STD) \
		$(COMPILE_FLAGS) \
		$(addprefix -I, $(INCLUDE_DIRS)) \
		-MF $@.d -MMD -MP \
		$(addprefix -D, $(COMPILE_DEFINITIONS)) \
		-o $@ $< \
	)

.PRECIOUS: $(OBJ_DIR)/%.o

$(BIN_DIR)/%: $(NODE_OBJS) $(OBJ_DIR)/%.o
	mkdir -p $(@D)
	$(strip \
		$(CXX) $(LINK_FLAGS) \
		$(addprefix -L, $(LIB_DIRS)) \
		$(addprefix -l, $(LIBS)) \
		$(addprefix -framework , $(OSX_FRAMEWORKS)) \
		-o $@ $+ \
	)

.PHONY: all exec main test run_test perf_test run_perf_test pdemo ct c clean

all: main

#exec: TARGETS :=
#exec: $(addprefix $(BIN_DIR)/, $(TARGETS))
#	scripts/run_commands_long_circuit.sh $+

main: $(BIN_DIR)/main

test: $(TESTS)

run_test: test
	scripts/run_commands_long_circuit.sh $(TESTS)

perf_test: $(BIN_DIR)/core_tests/performance_test

run_perf_test: perf_test
	$(BIN_DIR)/core_tests/performance_test

pdemo: $(BIN_DIR)/prometheus_demo
	$< $(PUSHGATEWAY_IP) $(PUSHGATEWAY_PORT) $(PUSHGATEWAY_NAME)

ct:
	rm -rf $(TESTS) $(TEST_OBJS)

c: clean

clean:
	rm -rf $(BUILD_DIR)
