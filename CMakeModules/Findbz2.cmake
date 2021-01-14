# - Try to find bz2 include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(bz2)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(bz2_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  bz2_FOUND             System has bz2, include and library dirs found
#  bz2_INCLUDE_DIRS      The bz2 include directories.
#  bz2_LIBRARIES         The bz2 library

set(LIB_SUFFIX .so)
if(bz2_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif()

find_path(bz2_INCLUDE_DIR
        NAMES bzlib.h
        PATH_SUFFIXES bz2/
        DOC "bz2 include dir"
        )

find_library(
        bz2_LIBRARY
        NAMES libbz2${LIB_SUFFIX} bz2
        DOC "bz2 library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(bz2 DEFAULT_MSG
        bz2_LIBRARY
        bz2_INCLUDE_DIR
        )

mark_as_advanced(
        bz2_INCLUDE_DIR
        bz2_LIBRARY
)

set(bz2_INCLUDE_DIRS ${bz2_INCLUDE_DIR})
set(bz2_LIBRARIES ${bz2_LIBRARY})