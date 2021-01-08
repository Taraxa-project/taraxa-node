ifndef Makefile_submodules
Makefile_submodules=_

include Makefile_common.mk

# NOTE: there's a supporting file with shell utils: Makefile_submodules.sh

# By default, sync submodules on every `make` invocation to make
# the developers think less about submodules
ifeq ($(UPDATE_SUBMODULES), 1)
ifneq (0, $(shell \
	source Makefile_submodules.sh; \
	submodule_upd >> Makefile.log.txt; \
	echo $$?; \
))
$(error Submodule update failed)
endif
endif

# NOTE: the general idea in the submodule building pattern is that
# once the submodule build is successful, we create a dummy file `ok`
# in the submodule directory and use it as a dependency in targets
# that need the submodule. Submodule builds always start from cleaning
# the submodule directory from files that are not tracked in git, thus
# these builds are atomic

# list of all submodule build success indicator files
SUBMODULE_DEPS := $(addsuffix /ok, $(shell \
	source Makefile_submodules.sh; \
	submodule_list; \
))

SUBMODULE_BUILD_BEGIN = \
	source Makefile_submodules.sh; \
	mkdir -p $(DEPS_INSTALL_PREFIX); \
	mkdir -p $(DEPS_INSTALL_PREFIX)/lib; \
	mkdir -p $(DEPS_INSTALL_PREFIX)/include; \
	cd $(@D); \
	git_clean
SUBMODULE_BUILD_END = \
	touch $(CURDIR)/$@

# ====================================================

# TODO explore more cgo compiler optimization flags
TARAXA_EVM_LIB := taraxa-evm
submodules/taraxa-evm/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	cd taraxa/C; \
	CGO_CFLAGS_ALLOW='.*' CGO_CXXFLAGS_ALLOW='.*' \
		CGO_CFLAGS="-O3" CGO_CXXFLAGS="-O3" \
		go build -buildmode=c-archive -o $(TARAXA_EVM_LIB).a; \
	copy . "*.h" $(DEPS_INSTALL_PREFIX)/include/taraxa-evm/; \
	cp $(TARAXA_EVM_LIB).a $(DEPS_INSTALL_PREFIX)/lib/lib$(TARAXA_EVM_LIB).a; \
	$(SUBMODULE_BUILD_END);

# ====================================================

TARAXA_VDF_OPTS :=
ifneq ($(SYSTEM_HOME_OVERRIDE),)
	TARAXA_VDF_OPTS += OPENSSL_HOME=$(SYSTEM_HOME_OVERRIDE)
endif
submodules/taraxa-vdf/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	$(MAKE) $(TARAXA_VDF_OPTS); \
	copy . "include/*.*" $(DEPS_INSTALL_PREFIX)/; \
	copy . "lib/*.*" $(DEPS_INSTALL_PREFIX)/; \
	$(SUBMODULE_BUILD_END);

# ====================================================

submodules/taraxa-vrf/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	autoreconf; \
	automake; \
	./configure --prefix=$(DEPS_INSTALL_PREFIX); \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

submodules/googletest/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	mkdir_cd build; \
	cmake -DCMAKE_INSTALL_PREFIX=$(DEPS_INSTALL_PREFIX) ..; \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

submodules/cryptopp/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	if [ $(OS) = 'Darwin' ]; then \
		$(MAKE); \
	else \
		$(MAKE) CXXFLAGS="-DNDEBUG -g2 -O3 -fPIC \
			$(addprefix -D, $(COMPILE_DEFINITIONS)) \
			-pthread -pipe -c"; \
	fi; \
	$(MAKE) PREFIX=$(DEPS_INSTALL_PREFIX) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

submodules/ethash/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	mkdir_cd build; \
	cmake -DCMAKE_INSTALL_PREFIX=$(DEPS_INSTALL_PREFIX) .. \
		-DBUILD_TESTING=OFF \
		-DBUILD_SHARED_LIBS=OFF \
		-DETHASH_BUILD_TESTS=OFF \
		-DHUNTER_ENABLED=OFF; \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

LIBFF_OPTS := \
	-DBUILD_TESTING=OFF \
	-DBUILD_SHARED_LIBS=OFF \
	-DWITH_PROCPS=OFF \
	-DCMAKE_C_COMPILER=gcc \
	-DCMAKE_CXX_COMPILER=c++
ifneq ($(SYSTEM_HOME_OVERRIDE),)
	LIBFF_OPTS += \
		-DOPENSSL_ROOT_DIR=$(SYSTEM_HOME_OVERRIDE) \
		-DOPENSSL_LIBRARIES=$(SYSTEM_HOME_OVERRIDE)/lib
endif
submodules/libff/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	mkdir_cd build; \
	cmake -DCMAKE_INSTALL_PREFIX=$(DEPS_INSTALL_PREFIX) .. $(LIBFF_OPTS); \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

submodules/secp256k1/ok:
	$(SUBMODULE_BUILD_BEGIN); \
	./autogen.sh; \
	./configure --prefix=$(DEPS_INSTALL_PREFIX) \
		--disable-shared --disable-tests \
		--disable-coverage --disable-openssl-tests \
		--disable-exhaustive-tests \
		--disable-jni --with-bignum=no --with-field=64bit \
		--with-scalar=64bit --with-asm=no \
		--enable-module-ecdh --enable-module-recovery \
		--enable-endomorphism \
		--enable-experimental; \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

ALETH_ROOT := $(CURDIR)/submodules/taraxa-aleth
ALETH_BUILD_DIR := $(ALETH_ROOT)/build
ALETH_OBJ_DIR := $(ALETH_BUILD_DIR)/obj
ALETH_SRCS := $(shell \
	find $(ALETH_ROOT) -path "$(ALETH_ROOT)/lib*/*.cpp" \
	-a ! -path "$(ALETH_ROOT)/libweb3jsonrpc/WinPipeServer.cpp" \
)
ALETH_OBJS = $(subst $(ALETH_ROOT),$(ALETH_OBJ_DIR),$(ALETH_SRCS:.cpp=.o))
TARAXA_ALETH_LIB := taraxa-aleth

# compile any taraxa-aleth .o file. uses configuration similar to the main build
$(ALETH_OBJ_DIR)/%.o: $(ALETH_ROOT)/%.cpp
	mkdir -p $(@D)
	$(strip \
		$(CXX) -c -std=$(CXX_STD) $(COMPILE_FLAGS) \
		$(addprefix -I, $(ALETH_ROOT) $(INCLUDE_DIRS)) \
		$(addprefix -D, $(COMPILE_DEFINITIONS)) \
		-o $@ $< \
	)

submodules/taraxa-aleth/ok: \
submodules/cryptopp/ok \
submodules/ethash/ok \
submodules/libff/ok \
submodules/secp256k1/ok
	$(SUBMODULE_BUILD_BEGIN); \
	$(MAKE) -C $(CURDIR) -f Makefile_submodules.mk UPDATE_SUBMODULES=0 \
		$(ALETH_OBJS); \
	ar -rcs $(DEPS_INSTALL_PREFIX)/lib/lib$(TARAXA_ALETH_LIB).a $(ALETH_OBJS); \
	copy $(ALETH_ROOT) "lib*/*.h" $(DEPS_INSTALL_PREFIX)/include; \
	$(SUBMODULE_BUILD_END);

# ====================================================

.PHONY: submodules

submodules: $(SUBMODULE_DEPS)

endif # Makefile_submodules
