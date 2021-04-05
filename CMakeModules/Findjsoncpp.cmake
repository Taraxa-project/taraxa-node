# - Try to find jsoncpp include dirs and libraries
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH
#
# Usage of this module as follows:
#
#     find_package(jsoncpp)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#     set(jsoncpp_USE_STATIC_LIBS ON/OFF)
#
# Variables defined by this module:
#
#  jsoncpp_FOUND             System has jsoncpp, include and library dirs found
#  jsoncpp_INCLUDE_DIRS      The crypto include directories.
#  jsoncpp_LIBRARIES         The crypto library


if (jsoncpp_INCLUDE_DIRS AND jsoncpp_LIBRARIES)
    set(jsoncpp_FIND_QUIETLY TRUE)
endif (jsoncpp_INCLUDE_DIRS AND jsoncpp_LIBRARIES)

find_path(jsoncpp_INCLUDE_DIR json/json.h PATH_SUFFIXES jsoncpp/)

if (jsoncpp_USE_STATIC_LIBS)
    find_library(jsoncpp_LIBRARY NAMES libjsoncpp.a jsoncpp)
else ()
    find_library(jsoncpp_LIBRARY NAMES libjsoncpp.so jsoncpp)
endif ()

set(jsoncpp_INCLUDE_DIRS ${jsoncpp_INCLUDE_DIR})
set(jsoncpp_LIBRARIES ${jsoncpp_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set jsoncpp_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jsoncpp DEFAULT_MSG jsoncpp_INCLUDE_DIR jsoncpp_LIBRARY)

mark_as_advanced(jsoncpp_INCLUDE_DIR jsoncpp_LIBRARY)
