# - Try to find libgmp include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(gmp)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(gmp_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  gmp_FOUND             System has gmp, include and library dirs found
#  gmp_INCLUDE_DIRS      The gmp include directories.
#  gmp_LIBRARIES         The gmp library

set(LIB_SUFFIX .so)
if (gmp_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif ()

find_path(gmp_INCLUDE_DIR
        NAMES gmpxx.h
        PATH_SUFFIXES gmp/
        DOC "gmp include dir"
        )

find_library(
        gmp_LIBRARY
        NAMES libgmp${LIB_SUFFIX} gmp
        DOC "gmp library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gmp DEFAULT_MSG
        gmp_LIBRARY
        gmp_INCLUDE_DIR
        )

mark_as_advanced(
        gmp_INCLUDE_DIR
        gmp_LIBRARY
)

set(gmp_INCLUDE_DIRS ${gmp_INCLUDE_DIR})
set(gmp_LIBRARIES ${gmp_LIBRARY})