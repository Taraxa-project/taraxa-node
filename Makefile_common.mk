ifndef Makefile_common
Makefile_common=_

# cleanup the conventional log file that can be used for logging in $(shell ...)
# statements. useful because the statement returns all the stdout as a value.
_ := $(shell rm -rf Makefile.log.txt)

# VARIABLES THAT YOU CAN OVERRIDE
# rather, which kind of bash do you prefer, cause this build relies on bash
SHELL := /bin/bash
DEBUG := 0
# rather, which kind of gcc do you prefer, cause this build relies on bash
CXX := g++
CXX_STD := c++17
OPENSSL_HOME :=
BUILD_BASEDIR = $(CURDIR)/build
DEPS_INSTALL_PREFIX = $(BUILD_BASEDIR)/deps
# needed only because jsoncpp packaging is not standard across OS
JSONCPP_INCLUDE_DIR := /usr/include/jsoncpp
INCLUDE_DIRS = $(JSONCPP_INCLUDE_DIR) $(DEPS_INSTALL_PREFIX)/include
LIB_DIRS = $(DEPS_INSTALL_PREFIX)/lib
# ideally these should be the same in our build and submodule builds
# to make sure every module sees the sources the same way
COMPILE_DEFINITIONS := \
	CRYPTOPP_DISABLE_ASM \
	BOOST_ALL_DYN_LINK \
	BOOST_SPIRIT_THREADSAFE
UPDATE_SUBMODULES := 1
# makefile with overrides,
# also you can put there custom local targets, which can even use variables
# from the main build
sinclude local/Makefile_ext.mk
# can't override this
OS := $(shell uname)
ifndef COMPILE_FLAGS
	ifeq ($(DEBUG), 1)
		COMPILE_FLAGS := -g -O0
	else
		COMPILE_FLAGS := -O3
	endif
endif
ifndef LINK_FLAGS
	LINK_FLAGS := -Wl,-rpath $(DEPS_INSTALL_PREFIX)/lib
	ifeq ($(DEBUG), 1)
		ifeq ($(OS), Darwin)
			LINK_FLAGS += -rdynamic
		else
			LINK_FLAGS += -Wl,--export-dynamic
		endif
	endif
endif
ifneq ($(OPENSSL_HOME),)
	INCLUDE_DIRS := $(OPENSSL_HOME)/include $(INCLUDE_DIRS)
	LIB_DIRS := $(OPENSSL_HOME)/lib $(LIB_DIRS)
endif
# END VARIABLES THAT YOU CAN OVERRIDE

endif # Makefile_common