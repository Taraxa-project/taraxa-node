SYSTEM_HOME_OVERRIDE?=
UPDATE_SUBMODULES?=1
DIST?=$(CURDIR)/__dist__
COMPILE_DEFINITIONS?=

# NOTE: there's a supporting file with shell utils: Makefile.sh
# By default, sync submodules on every `make` invocation to make
# the developers think less about submodules
ifeq ($(UPDATE_SUBMODULES), 1)
ifneq (0, $(shell \
	source Makefile.sh; \
	submodule_upd &> Makefile.log.txt; \
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
SUBMODULE_BUILD_BEGIN = \
	source Makefile.sh; \
	export NAME=$(basename $(@F)); \
	export SRC_DIR=$(DIST)/src_$${NAME}; \
	export STAGE_DIR=$(DIST)/dist_$${NAME}; \
	mkdir -p $(DIST); \
	mkdir -p $${STAGE_DIR}; \
	mkdir -p $${STAGE_DIR}/lib; \
	mkdir -p $${STAGE_DIR}/include; \
	mkdir -p $${STAGE_DIR}/bin; \
	(cat $@ 2>/dev/null || true) | while read rel_path; do \
		rm -rf "$(DIST)/${rel_path}"; \
	done; \
	rm -rf $@; \
	rm -rf $${SRC_DIR}; \
	mkdir_cd $${SRC_DIR}; \
	cp -r $(CURDIR)/$${NAME}/* ./
SUBMODULE_BUILD_END = \
	rm -rf $${SRC_DIR}; \
	cd $${STAGE_DIR}; \
	cp -r * $(DIST)/; \
	find * -type f &> $@; \
	rm -rf $${STAGE_DIR}

# ====================================================

# TODO explore more cgo compiler optimization flags
$(DIST)/taraxa-evm.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	cd taraxa/C; \
	CGO_CFLAGS_ALLOW='.*' CGO_CXXFLAGS_ALLOW='.*' \
		CGO_CFLAGS="-O3" CGO_CXXFLAGS="-O3" \
		go build \
			-buildmode=c-archive \
			-ldflags="-linkmode external -extldflags=-static" \
			-tags=static \
			-o $${NAME}.a; \
	copy . "*.h" $${STAGE_DIR}/include/$${NAME}/; \
	cp $${NAME}.a $${STAGE_DIR}/lib/lib$${NAME}.a; \
	$(SUBMODULE_BUILD_END);

# ====================================================

TARAXA_VDF_OPTS :=
ifneq ($(SYSTEM_HOME_OVERRIDE),)
	TARAXA_VDF_OPTS += OPENSSL_HOME=$(SYSTEM_HOME_OVERRIDE)
endif
$(DIST)/taraxa-vdf.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	$(MAKE) $(TARAXA_VDF_OPTS); \
	copy . "include/*.*" $${STAGE_DIR}/; \
	copy . "lib/*.*" $${STAGE_DIR}/; \
	$(SUBMODULE_BUILD_END);

# ====================================================

$(DIST)/taraxa-vrf.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	autoreconf; \
	automake; \
	./configure --enable-static="yes" --enable-shared="no" --prefix=$${STAGE_DIR}; \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

$(DIST)/googletest.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	mkdir_cd build; \
	cmake -DCMAKE_INSTALL_PREFIX=$${STAGE_DIR} ..; \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

$(DIST)/cryptopp.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	$(MAKE) CXXFLAGS="-DNDEBUG -O3 -fPIC \
		$(addprefix -D, $(COMPILE_DEFINITIONS)) \
		-fvisibility=hidden -pthread -pipe -c"; \
	$(MAKE) PREFIX=$${STAGE_DIR} install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

$(DIST)/ethash.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	mkdir_cd build; \
	cmake -DCMAKE_INSTALL_PREFIX=$${STAGE_DIR} .. \
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
$(DIST)/libff.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	mkdir_cd build; \
	cmake -DCMAKE_INSTALL_PREFIX=$${STAGE_DIR} .. $(LIBFF_OPTS); \
	$(MAKE); \
	$(MAKE) install; \
	$(SUBMODULE_BUILD_END);

# ====================================================

$(DIST)/secp256k1.ok:
	$(SUBMODULE_BUILD_BEGIN); \
	./autogen.sh; \
	./configure --prefix=$${STAGE_DIR} \
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

ALETH_ROOT := $(DIST)/src_taraxa-aleth
ALETH_OBJ_DIR := $(ALETH_ROOT)/build/obj
ALETH_SRCS := $(shell \
	cd taraxa-aleth; find * -path "lib*/*.cpp" \
	-a ! -path "libweb3jsonrpc/WinPipeServer.cpp" \
)
ALETH_OBJS = $(addprefix $(ALETH_OBJ_DIR)/, $(ALETH_SRCS:.cpp=.o))
ALETH_INCLUDE_DIRS = $(ALETH_ROOT) $(ALETH_ROOT)/utils /usr/include/jsoncpp $(DIST)/include
ifneq ($(SYSTEM_HOME_OVERRIDE),)
	ALETH_INCLUDE_DIRS += $(SYSTEM_HOME_OVERRIDE)/include
endif

# compile any taraxa-aleth .o file. uses configuration similar to the main build
$(ALETH_OBJ_DIR)/%.o: $(ALETH_ROOT)/%.cpp
	mkdir -p $(@D)
	$(strip \
		g++ -c -std=c++17 -O3 -fvisibility=hidden \
		$(addprefix -I,  $(ALETH_INCLUDE_DIRS)) \
		$(addprefix -D, $(COMPILE_DEFINITIONS)) \
		-o $@ $< \
	)

$(DIST)/taraxa-aleth.ok: \
$(DIST)/cryptopp.ok \
$(DIST)/ethash.ok \
$(DIST)/libff.ok \
$(DIST)/secp256k1.ok
	$(SUBMODULE_BUILD_BEGIN); \
	copy utils "*.h" $${STAGE_DIR}/include; \
	$(MAKE) -C $(CURDIR) UPDATE_SUBMODULES=0 $(ALETH_OBJS); \
	ar -rcs $${STAGE_DIR}/lib/lib$${NAME}.a $(ALETH_OBJS); \
	copy . "lib*/*.h" $${STAGE_DIR}/include; \
	$(SUBMODULE_BUILD_END);

# ====================================================

SUBMODULES := $(shell grep path ../.gitmodules | sed 's/.*= submodules\///')

define DECLARE_DEPENDENCIES
$(DIST)/$(1).ok: $(CURDIR)/$(1)
endef
$(foreach SUBMODULE_NAME, $(SUBMODULES), $(eval $(call DECLARE_DEPENDENCIES, $(SUBMODULE_NAME))))

submodules: $(addprefix $(DIST)/, $(addsuffix .ok, $(SUBMODULES)))
	@:

only_%: $(DIST)/%.ok
	@:

.PHONY: submodules only_%
.DEFAULT_GOAL := submodules