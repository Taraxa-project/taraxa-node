# - Try to find zstd include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(zstd)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(zstd_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  zstd_FOUND             System has zstd, include and library dirs found
#  zstd_INCLUDE_DIRS      The zstd include directories.
#  zstd_LIBRARIES         The zstd library

set(LIB_SUFFIX .so)
if(zstd_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif()

find_path(zstd_INCLUDE_DIR
        NAMES zstd.h
        PATH_SUFFIXES zstd/
        DOC "zstd include dir"
        )

find_library(
        zstd_LIBRARY
        NAMES libzstd${LIB_SUFFIX} zstd
        DOC "zstd library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(zstd DEFAULT_MSG
        zstd_LIBRARY
        zstd_INCLUDE_DIR
        )

mark_as_advanced(
        zstd_INCLUDE_DIR
        zstd_LIBRARY
)

set(zstd_INCLUDE_DIRS ${zstd_INCLUDE_DIR})
set(zstd_LIBRARIES ${zstd_LIBRARY})