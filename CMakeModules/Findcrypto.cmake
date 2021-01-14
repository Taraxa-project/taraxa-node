# - Try to find libcrypto include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(crypto)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(crypto_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  crypto_FOUND             System has crypto, include and library dirs found
#  crypto_INCLUDE_DIRS      The crypto include directories.
#  crypto_LIBRARIES         The crypto library

set(LIB_SUFFIX .so)
if(crypto_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif()

find_path(crypto_INCLUDE_DIR
        NAMES openssl/crypto.h
        PATH_SUFFIXES crypto/
        DOC "crypto include dir"
        )

find_library(
        crypto_LIBRARY
        NAMES libcrypto${LIB_SUFFIX} crypto
        DOC "crypto library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(crypto DEFAULT_MSG
        crypto_LIBRARY
        crypto_INCLUDE_DIR
        )

mark_as_advanced(
        crypto_ROOT_DIR
        crypto_INCLUDE_DIR
        crypto_LIBRARY
)

set(crypto_INCLUDE_DIRS ${crypto_INCLUDE_DIR})
set(crypto_LIBRARIES ${crypto_LIBRARY})