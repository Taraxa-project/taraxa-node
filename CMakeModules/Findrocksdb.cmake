# - Try to find rocksdb include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(rocksdb)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(rocksdb_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  rocksdb_FOUND             System has rocksdb, include and library dirs found
#  rocksdb_INCLUDE_DIRS      The rocksdb include directories.
#  rocksdb_LIBRARIES         The rocksdb library

set(LIB_SUFFIX .so)
if(rocksdb_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif()

find_path(rocksdb_INCLUDE_DIR
        NAMES rocksdb/db.h
        PATH_SUFFIXES rocksdb/
        DOC "rocksdb include dir"
        )

find_library(
        rocksdb_LIBRARY
        NAMES librocksdb${LIB_SUFFIX} rocksdb
        DOC "rocksdb library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(rocksdb DEFAULT_MSG
        rocksdb_LIBRARY
        rocksdb_INCLUDE_DIR
        )

mark_as_advanced(
        rocksdb_INCLUDE_DIR
        rocksdb_LIBRARY
)

set(rocksdb_INCLUDE_DIRS ${rocksdb_INCLUDE_DIR})
set(rocksdb_LIBRARIES ${rocksdb_LIBRARY})