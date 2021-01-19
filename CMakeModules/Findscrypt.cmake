# - Try to find libscrypt include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(scrypt)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(scrypt_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  scrypt_FOUND             System has scrypt, include and library dirs found
#  scrypt_INCLUDE_DIRS      The scrypt include directories.
#  scrypt_LIBRARIES         The scrypt library

set(LIB_SUFFIX .so)
if (scrypt_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif ()

find_path(scrypt_INCLUDE_DIR
        NAMES libscrypt.h
        PATH_SUFFIXES scrypt/
        DOC "scrypt include dir"
        )

find_library(
        scrypt_LIBRARY
        NAMES libscrypt${LIB_SUFFIX} scrypt
        DOC "scrypt library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(scrypt DEFAULT_MSG
        scrypt_LIBRARY
        scrypt_INCLUDE_DIR
        )

mark_as_advanced(
        scrypt_INCLUDE_DIR
        scrypt_LIBRARY
)

set(scrypt_INCLUDE_DIRS ${scrypt_INCLUDE_DIR})
set(scrypt_LIBRARIES ${scrypt_LIBRARY})