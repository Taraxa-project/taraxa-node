include Makefile_common.mk Makefile_submodules.mk

# NOTE: to understand $@, $(@D), $+, etc. variables,
# google 'make automatic variables'
# NOTE: makefile translates `$$?` into `$?`

# include gcc-generated makefiles that declare dependencies between
# and .cpp and .hpp files
include $(shell find $(BUILD_DIR) -path "*.d" 2> /dev/null)

BUILD_DIR := $(BUILD_BASEDIR)/release
ifneq ($(DEBUG), 0)
	BUILD_DIR := $(BUILD_BASEDIR)/debug
endif
SRC_DIR := $(CURDIR)/src
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
# filter out files in the src dir and cache in a local file.
# this way we save time on walking directories and also because of the 
# OS limits on command line length, it's better to use piping
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
TEST_SRC_QUALIFIER := _test
NODE_SRCS := $(shell $(FILES_CXX_PRINT) | \
	grep ".*.cpp" | \
	grep -v ".*$(TEST_SRC_QUALIFIER)*" | \
	grep -v "$(SRC_DIR)/main.cpp" \
)
NODE_OBJS := $(subst $(SRC_DIR),$(OBJ_DIR),$(NODE_SRCS:.cpp=.o))
TESTUTIL_SRCS := $(shell $(FILES_CXX_PRINT) | \
	grep "$(SRC_DIR)/util_$(TEST_SRC_QUALIFIER)/*.cpp" \
)
TESTUTIL_OBJS := $(subst $(SRC_DIR),$(OBJ_DIR),$(TESTUTIL_SRCS:.cpp=.o))
TEST_SRCS := $(shell $(FILES_CXX_PRINT) | \
	grep ".*$(TEST_SRC_QUALIFIER).cpp" \
)
TESTS := $(basename $(subst $(SRC_DIR),$(BIN_DIR),$(TEST_SRCS:.cpp=)))

# Compile any .o file, also generating .d makefiles that
# dependencies between the compiled files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(SUBMODULE_DEPS)
	mkdir -p $(@D) && $(strip \
		$(CXX) -c -std=$(CXX_STD) \
		$(COMPILE_FLAGS) \
		$(addprefix -I, $(SRC_DIR) $(INCLUDE_DIRS)) \
		-MF $@.d -MMD -MP \
		$(addprefix -D, $(COMPILE_DEFINITIONS)) \
		-o $@ $< \
	)
# Make sure .o files are not deleted automatically as intermediate results
.PRECIOUS: $(OBJ_DIR)/%.o

BOOST_LIBS := \
	boost_program_options \
	boost_filesystem \
	boost_system
# on Mac, brew boost distribution has that -mt (multithreaded) suffix for some libs
ifeq ($(BOOST_MT_SUFFIX), 1)
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
	ethash \
	$(SYS_LIBS)

# needed for golang runtime that comes together with taraxa-evm
ifeq ($(OS), Darwin)
	OSX_FRAMEWORKS := CoreFoundation Security
endif

TEST_LIBS := \
	gtest \
	jsonrpccpp-client

# the purpose of this is to escape the comma
_Wl_rpath_ := -Wl,-rpath
# base linking command
LINK = \
mkdir -p $(@D) && $(strip \
	$(CXX) $(LINK_FLAGS) $+ -o $@ \
	$(addprefix -L, $(LIB_DIRS)) \
	$(addprefix -l, $(LIBS)) \
	$(addprefix -framework , $(OSX_FRAMEWORKS)) \
	$(addprefix $(_Wl_rpath_) , $(LIB_DIRS)) \
)
$(BIN_DIR)/main: $(NODE_OBJS) $(OBJ_DIR)/main.o
	$(LINK)
# link any test, outputting to
# {binary out dir}/{name of test file without extension relative to the src dir}
$(BIN_DIR)/%$(TEST_SRC_QUALIFIER): $(NODE_OBJS) $(TESTUTIL_OBJS) $(OBJ_DIR)/%_test.o
	$(LINK) $(addprefix -l, $(TEST_LIBS))

# targets that don't produce real files
.PHONY: bin/% main test run_test print_% check \
fmt fmtcheck lint all clean c ct gen_rpc nothing
# do not log shell instructions themselves inside these targets
.SILENT: fmt fmtcheck lint

# `bin` is a pseudo-directory that resolves to the real binary out dir for
# the current configuration
bin/%: $(BIN_DIR)/%
	@:

main: bin/main
	@:

all: main
	@:

test: $(TESTS)
	@:

# update jsonrpccpp generated stubs from json specs
gen_rpc:
	src/net/gen_rpc.sh

run_test: $(TESTS)
	GODEBUG=cgocheck=0 scripts/run_commands_long_circuit.sh $+

# format all files inside the src dir
fmt:
	$(FILES_CXX_PRINT) | scripts/clang_format.sh

fmtcheck:
	@echo Validating C++ formatting...
	$(FILES_CXX_PRINT) | scripts/clang_format_validate.sh

# run static analysis on all non-test files
lint:
	@echo Running cppcheck...
	$(FILES_CXX_PRINT) | \
      grep -v ".*$(TEST_SRC_QUALIFIER)*" | \
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
        --suppress=unreadVariable \
        --suppress=unknownMacro \
        --suppress=useInitializationList \
        --suppress=syntaxError \
        1>/dev/null

check: fmtcheck lint
	@:

ct:
	rm -rf $(TESTS) $(TEST_OBJS) $(TESTUTIL_OBJS)

c: clean
	@:

clean:
	rm -rf $(BUILD_DIR)

# usage: `make print_${VARIABLE_NAME}`
# prints any variable inside this makefile. can be used to borrow configuration
# from this build in another build e.g. Cmake
print_%:
	@echo $($(subst print_,,$@))

nothing:
	@:

.DEFAULT_GOAL := nothing
