include Makefile_common.mk Makefile_submodules.mk

include $(shell find $(BUILD_DIR) -path "*.d" 2> /dev/null)

BUILD_DIR := $(BUILD_BASEDIR)/release
ifneq ($(DEBUG), 0)
	BUILD_DIR := $(BUILD_BASEDIR)/debug
endif
SRC_DIR := $(CURDIR)/src
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
_ := $(shell find "$(SRC_DIR)" -type f \
	-path "*.cpp" \
	-or -path "*.cxx" \
	-or -path "*.cc" \
	-or -path "*.c" \
	-or -path "*.hpp" \
	-or -path "*.hxx" \
	-or -path "*.hh" \
	-or -path "*.h" \
	&> Makefile.src_cxx.txt \
)
FILES_CXX_PRINT := cat "$(CURDIR)/Makefile.src_cxx.txt"
NODE_SRCS := $(shell $(FILES_CXX_PRINT) | \
	grep ".*.cpp" | \
	grep -v ".*_test*" | \
	grep -v "$(SRC_DIR)/main.cpp" \
)
NODE_OBJS := $(subst $(SRC_DIR),$(OBJ_DIR),$(NODE_SRCS:.cpp=.o))
TESTUTIL_SRCS := $(shell $(FILES_CXX_PRINT) | \
	grep "$(SRC_DIR)/util_test/.*.cpp" \
)
TESTUTIL_OBJS := $(subst $(SRC_DIR),$(OBJ_DIR),$(TESTUTIL_SRCS:.cpp=.o))
TEST_SRCS := $(shell $(FILES_CXX_PRINT) | \
	grep ".*_test.cpp" \
)
TESTS := $(basename $(subst $(SRC_DIR),$(BIN_DIR),$(TEST_SRCS:.cpp=)))

# Compile
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(SUBMODULE_DEPS)
	mkdir -p $(@D) && $(strip \
		$(CXX) -c -std=$(CXX_STD) \
		$(COMPILE_FLAGS) \
		$(addprefix -I, $(SRC_DIR) $(INCLUDE_DIRS)) \
		-MF $@.d -MMD -MP \
		$(addprefix -D, $(COMPILE_DEFINITIONS)) \
		-o $@ $< \
	)
.PRECIOUS: $(OBJ_DIR)/%.o

BOOST_LIBS := \
	boost_program_options \
	boost_filesystem \
	boost_system
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
	jsoncpp \
	jsonrpccpp-common \
	jsonrpccpp-server \
	ff \
	secp256k1 \
	cryptopp \
	ethash
TEST_LIBS := \
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

LINK = \
mkdir -p $(@D) && $(strip \
	$(CXX) $(LINK_FLAGS) $+ -o $@ \
	$(addprefix -L, $(LIB_DIRS)) \
	$(addprefix -l, $(LIBS)) \
	$(addprefix -framework , $(OSX_FRAMEWORKS)) \
)
$(BIN_DIR)/main: $(NODE_OBJS) $(OBJ_DIR)/main.o
	$(LINK)
$(BIN_DIR)/%_test: $(NODE_OBJS) $(TESTUTIL_OBJS) $(OBJ_DIR)/%_test.o
	$(LINK) $(addprefix -l, $(TEST_LIBS))

.PHONY: bin/% main test run_test print_% fmt fmtcheck lint all clean c ct gen_rpc
.SILENT: fmt fmtcheck lint

bin/%: $(BIN_DIR)/%
	@:

main: bin/main
	@:

all: main
	@:

test: $(TESTS)
	@:

gen_rpc:
	src/net/gen_rpc.sh

run_test: $(TESTS)
	GODEBUG=cgocheck=0 scripts/run_commands_long_circuit.sh $+

fmt:
	$(FILES_CXX_PRINT) | scripts/clang_format.sh

fmtcheck:
	@echo Validating C++ formatting...
	$(FILES_CXX_PRINT) | scripts/clang_format_validate.sh

lint:
	@echo Running cppcheck...
	$(FILES_CXX_PRINT) | \
      grep -v ".*_test*" | \
      cppcheck \
        --file-list=- \
        --error-exitcode=1 \
        --enable=warning,style,performance,portability,information \
        --suppress=missingInclude \
        --suppress=useStlAlgorithm \
        --suppress=shadowVariable \
        --suppress=unusedStructMember \
        --suppress=stlIfFind \
        --suppress=identicalConditionAfterEarlyExit \
        --suppress=noCopyConstructor \
        --suppress=noExplicitConstructor \
        --suppress=passedByValue \
        --suppress=unreadVariable \
        --suppress=unknownMacro \
        --suppress=useInitializationList \
        --suppress=syntaxError \
        1>/dev/null

ct:
	rm -rf $(TESTS) $(TEST_OBJS) $(TESTUTIL_OBJS)

c: clean
	@:

clean:
	rm -rf $(BUILD_DIR)

# usage: `make print_${VARIABLE_NAME}`
# Can be used to borrow configuration from this build
# in another build e.g. Cmake
print_%:
	@echo $($(subst print_,,$@))

.DEFAULT_GOAL := main