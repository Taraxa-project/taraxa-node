# - Try to find lz4 include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(lz4)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(lz4_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  lz4_FOUND             System has lz4, include and library dirs found
#  lz4_INCLUDE_DIRS      The lz4 include directories.
#  lz4_LIBRARIES         The lz4 library

set(LIB_SUFFIX .so)
if (lz4_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif ()

find_path(lz4_INCLUDE_DIR
        NAMES lz4.h
        PATH_SUFFIXES lz4/
        DOC "lz4 include dir"
        )

find_library(
        lz4_LIBRARY
        NAMES liblz4${LIB_SUFFIX} lz4
        DOC "lz4 library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(lz4 DEFAULT_MSG
        lz4_LIBRARY
        lz4_INCLUDE_DIR
        )

mark_as_advanced(
        lz4_INCLUDE_DIR
        lz4_LIBRARY
)

set(lz4_INCLUDE_DIRS ${lz4_INCLUDE_DIR})
set(lz4_LIBRARIES ${lz4_LIBRARY})