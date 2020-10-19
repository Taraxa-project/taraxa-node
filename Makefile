include Makefile.common
include Makefile.submodules

include $(shell find $(BUILD_DIR) -path "*.d" 2> /dev/null)

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
	$(CRYPTOPP_COMPILE_DEFIINITIONS)

INCLUDE_DIRS := $(CURDIR) $(DEPS_INSTALL_PREFIX)/include $(JSONCPP_INCLUDE_DIR)

LINK_FLAGS := -Wl,-rpath $(DEPS_INSTALL_PREFIX)/lib
ifeq ($(DEBUG), 1)
	ifeq ($(OS), Darwin)
		LINK_FLAGS += -rdynamic
	else
		LINK_FLAGS += -Wl,--export-dynamic
	endif
endif

LIB_DIRS := $(DEPS_INSTALL_PREFIX)/lib

BOOST_LIBS := \
	boost_program_options \
	boost_filesystem \
	boost_system \

ifeq ($(OS), Darwin)
	BOOST_LIBS += boost_thread-mt boost_log-mt boost_log_setup-mt
else
	BOOST_LIBS += boost_thread boost_log boost_log_setup
endif
LIBS := \
	$(TARAXA_ALETH_LIB) \
	$(TARAXA_EVM_LIB) \
	vdf \
	sodium \
	dl \
	pthread \
	z \
	curl \
	ssl \
	crypto \
	gmp \
	gmpxx \
	mpfr \
	$(BOOST_LIBS) \
	rocksdb \
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

NODE_SRCS := $(shell scripts/find_files_cxx_node_main.sh)
NODE_OBJS := $(addprefix $(OBJ_DIR)/, $(NODE_SRCS:.cpp=.o))
TEST_SRCS := $(shell scripts/find_files_cxx_node_tests.sh)
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
		$(CXX) $(LINK_FLAGS) $+ -o $@ \
		$(addprefix -L, $(LIB_DIRS)) \
		$(addprefix -l, $(LIBS)) \
		$(addprefix -framework , $(OSX_FRAMEWORKS)) \
	)

.PHONY: all main test run_test perf_test run_perf_test pdemo ct c clean \
	print_var

all: main

main: $(BIN_DIR)/main

test: $(TESTS)

cppcheck_test:
	cppcheck --enable=warning,style,performance,portability,information --error-exitcode=1 -i submodules -i core_tests -i unused_yet_useful -i prometheus_demo.cpp --suppress=missingInclude .

run_test: test
	GODEBUG=cgocheck=0 scripts/run_commands_long_circuit.sh $(TESTS)

perf_test: $(BIN_DIR)/core_tests/performance_test
crypto_test: $(BIN_DIR)/core_tests/crypto_test

run_perf_test: perf_test
	$(BIN_DIR)/core_tests/performance_test

pdemo: $(BIN_DIR)/prometheus_demo
	$< $(PUSHGATEWAY_IP) $(PUSHGATEWAY_PORT) $(PUSHGATEWAY_NAME)

ct:
	rm -rf $(TESTS) $(TEST_OBJS)

c: clean

clean:
	rm -rf $(BUILD_DIR)

# Can be used to borrow configuration from this build
# to another build e.g. Cmake
print_var:
	@echo $($(ARG))
