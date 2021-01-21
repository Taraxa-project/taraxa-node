# - Try to find libmpfr include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(mpfr)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(mpfr_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  mpfr_FOUND             System has mpfr, include and library dirs found
#  mpfr_INCLUDE_DIRS      The mpfr include directories.
#  mpfr_LIBRARIES         The mpfr library

set(LIB_SUFFIX .so)
if (mpfr_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif ()

find_path(mpfr_INCLUDE_DIR
        NAMES mpfr.h
        PATH_SUFFIXES mpfr/
        DOC "mpfr include dir"
        )

find_library(
        mpfr_LIBRARY
        NAMES libmpfr${LIB_SUFFIX} mpfr
        DOC "mpfr library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(mpfr DEFAULT_MSG
        mpfr_LIBRARY
        mpfr_INCLUDE_DIR
        )

mark_as_advanced(
        mpfr_INCLUDE_DIR
        mpfr_LIBRARY
)

set(mpfr_INCLUDE_DIRS ${mpfr_INCLUDE_DIR})
set(mpfr_LIBRARIES ${mpfr_LIBRARY})