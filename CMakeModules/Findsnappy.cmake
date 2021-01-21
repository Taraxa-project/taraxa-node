# - Try to find snappy include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(snappy)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(snappy_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  snappy_FOUND             System has snappy, include and library dirs found
#  snappy_INCLUDE_DIRS      The snappy include directories.
#  snappy_LIBRARIES         The snappy library

set(LIB_SUFFIX .so)
if (snappy_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif ()

find_path(snappy_INCLUDE_DIR
        NAMES snappy.h
        PATH_SUFFIXES snappy/
        DOC "snappy include dir"
        )

find_library(
        snappy_LIBRARY
        NAMES libsnappy${LIB_SUFFIX} snappy
        DOC "snappy library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(snappy DEFAULT_MSG
        snappy_LIBRARY
        snappy_INCLUDE_DIR
        )

mark_as_advanced(
        snappy_INCLUDE_DIR
        snappy_LIBRARY
)

set(snappy_INCLUDE_DIRS ${snappy_INCLUDE_DIR})
set(snappy_LIBRARIES ${snappy_LIBRARY})