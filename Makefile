include Makefile.common
include Makefile.submodules

DEPS := $(SUBMODULE_DEPS)

DEBUG = 0
PERF = 0

COMPILE_FLAGS := -O3
ifneq ($(DEBUG), 0)
	COMPILE_FLAGS := -g
endif
ifneq ($(PERF), 0)
 	COMPILE_FLAGS += -fno-omit-frame-pointer
endif

BOOST_LIBS := \
	-lboost_program_options \
	-lboost_filesystem \
	-lboost_system \
	-lboost_thread-mt \
	-lboost_log_setup
ifeq ($(OS), Darwin)
	BOOST_LIBS += -lboost_log-mt
else
	BOOST_LIBS += -lboost_log
endif

INCLUDE_DIRS := \
	-I$(CURDIR) \
	$(ALETH_TRANSITIVE_INCLUDE_DIRS) \
	-I$(ALETH_INSTALL_PREFIX)/include \
	-I$(OPENSSL_INSTALL_PREFIX)/include \
	-I$(TARAXA_VRF_INSTALL_PREFIX)/include \
	-I$(TARAXA_VDF_INSTALL_PREFIX)/include \
	-I$(PROMETHEUS_CPP_INSTALL_PREFIX)/include \
	-I$(RAPIDJSON_INCLUDE_DIR) \
	-I$(TARAXA_EVM_INCLUDE_DIR) \
	-I$(GTEST_INSTALL_PREFIX)/include

LIB_DIRS := \
	-L$(OPENSSL_INSTALL_PREFIX)/lib \
	-L$(TARAXA_VRF_INSTALL_PREFIX)/lib \
	-L$(TARAXA_VDF_INSTALL_PREFIX)/lib \
	-L$(CRYPTOPP_INSTALL_PREFIX)/lib \
	-L$(ETHASH_INSTALL_PREFIX)/lib \
	-L$(LIBFF_INSTALL_PREFIX)/lib \
	-L$(SECP256K1_INSTALL_PREFIX)/lib \
	-L$(ALETH_INSTALL_PREFIX)/lib \
	-L$(TARAXA_EVM_LIB_DIR) \
	-L$(PROMETHEUS_CPP_INSTALL_PREFIX)/lib \
	-L$(GTEST_INSTALL_PREFIX)/lib

LIBS := \
	-lpthread \
	-lz \
	-lcurl \
	-lssl \
	-lcrypto \
	-lgmp \
	-lgmpxx \
	-lmpfr \
	$(BOOST_LIBS) \
	-lleveldb \
	-lrocksdb \
	-lsodium \
	-lvdf \
	-lscrypt \
	-lprometheus-cpp-core \
	-lprometheus-cpp-push \
	-lprometheus-cpp-pull \
	-ljsoncpp \
	-ljsonrpccpp-common \
	-ljsonrpccpp-server \
	-lff \
	-lsecp256k1 \
	-lcryptopp \
	-lethash \
	$(addprefix -l, $(ALETH_LIBS)) \
	-l$(TARAXA_EVM_LIB) \
	-lgtest
# Optional linking for libatomic (part of standard library).
# Some toolchains provide this library,
# and assume programs using <atomic> would link against it.
# Note: makefile translates `$$?` into `$?`
LIBATOMIC_NOT_FOUND = $(shell \
    $(CXX) -latomic -shared -o /dev/null &> /dev/null; echo $$? \
)
ifeq ($(LIBATOMIC_NOT_FOUND), 0)
    LIBS += -latomic
endif
ifeq ($(OS), Darwin)
	LIBS += -framework CoreFoundation -framework Security
endif

LINK_FLAGS := -Wl,-rpath $(OPENSSL_INSTALL_PREFIX)/lib
ifneq ($(DEBUG), 0)
	ifeq ($(OS),Dawrin)
		LINK_FLAGS += -Wl,--export-dynamic
	else
		LINK_FLAGS += -rdynamic
	endif
endif

BUILD_DIR := build
ifneq ($(DEBUG), 0)
	BUILD_DIR := build-d
endif
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

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
	$(CXX) -c $(CXX_STD) $(COMPILE_FLAGS) \
		$(INCLUDE_DIRS) \
		$(BOOST_COMPILE_DEFINITIONS) $(ALETH_COMPILE_DEFINITIONS) \
		$(CRYPTOPP_COMPILE_DEFIINITIONS) -o $@ $<

.PRECIOUS: $(OBJ_DIR)/%.o

$(BIN_DIR)/%: $(NODE_OBJS) $(OBJ_DIR)/%.o
	mkdir -p $(@D)
	$(CXX) $(LINK_FLAGS) $(LIB_DIRS) $(LIBS) -o $@ $+


.PHONY: all main test run_test perf_test run_perf_test pdemo ct c clean

all: main

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
