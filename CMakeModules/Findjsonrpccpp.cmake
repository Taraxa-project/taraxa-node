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
#  jsonrpccpp_INCLUDE_DIRS, where to find header, etc.
#  jsonrpccpp_LIBRARIES, the libraries needed to use json-rpc-cpp.
#  jsonrpccpp_SERVER_LIBRARIES, the libraries needed to use json-rpc-cpp-server
#  jsonrpccpp_CLIENT_LIBRARIES, the libraries needed to use json-rpc-cpp-client
#  jsonrpccpp_FOUND, If false, do not try to use json-rpc-cpp.
#  jsonrpccpp_VERSION, version of library
#  jsonrpccpp_VERSION_MAJOR
#  jsonrpccpp_VERSION_MINOR
#  jsonrpccpp_VERSION_PATCH

set(LIB_SUFFIX .so)
if (jsonrpccpp_USE_STATIC_LIBS)
    set(LIB_SUFFIX .a)
endif ()

# only look in default directories
find_path(
        jsonrpccpp_INCLUDE_DIR
        NAMES jsonrpccpp/server.h jsonrpc/server.h
        PATH_SUFFIXES jsonrpc
        DOC "jsonrpccpp include dir"
)

find_library(
        jsonrpccpp_COMMON_LIBRARY
        NAMES libjsonrpccpp-common${LIB_SUFFIX} jsonrpccpp-common
        DOC "jsonrpccpp common library"
)

find_library(
        jsonrpccpp_SERVER_LIBRARY
        NAMES libjsonrpccpp-server${LIB_SUFFIX} jsonrpccpp-server
        DOC "jsonrpccpp server library"
)

find_library(
        jsonrpccpp_CLIENT_LIBRARY
        NAMES libjsonrpccpp-client${LIB_SUFFIX} jsonrpccpp-client
        DOC "jsonrpccpp client library"
)

# these are the variables to be uses by the calling script
set(jsonrpccpp_INCLUDE_DIRS ${jsonrpccpp_INCLUDE_DIR})
set(jsonrpccpp_LIBRARIES ${jsonrpccpp_COMMON_LIBRARY} ${jsonrpccpp_SERVER_LIBRARY} ${jsonrpccpp_CLIENT_LIBRARY})
set(jsonrpccpp_SERVER_LIBRARIES ${jsonrpccpp_COMMON_LIBRARY} ${jsonrpccpp_SERVER_LIBRARY})
set(jsonrpccpp_CLIENT_LIBRARIES ${jsonrpccpp_COMMON_LIBRARY} ${jsonrpccpp_CLIENT_LIBRARY})

if (jsonrpccpp_INCLUDE_DIR)
    set(jsonrpccpp_VERSION_HEADER "${jsonrpccpp_INCLUDE_DIR}/jsonrpccpp/version.h")
    if (EXISTS ${jsonrpccpp_VERSION_HEADER})
        file(STRINGS ${jsonrpccpp_VERSION_HEADER} jsonrpccpp_VERSION_MAJOR REGEX "^#define JSONRPC_CPP_MAJOR_VERSION[ \t]+[0-9]+$")
        file(STRINGS ${jsonrpccpp_VERSION_HEADER} jsonrpccpp_VERSION_MINOR REGEX "^#define JSONRPC_CPP_MINOR_VERSION[ \t]+[0-9]+$")
        file(STRINGS ${jsonrpccpp_VERSION_HEADER} jsonrpccpp_VERSION_PATCH REGEX "^#define JSONRPC_CPP_PATCH_VERSION[ \t]+[0-9]+$")
        string(REGEX REPLACE "^#define JSONRPC_CPP_MAJOR_VERSION[ \t]+([0-9]+)" "\\1" jsonrpccpp_VERSION_MAJOR ${jsonrpccpp_VERSION_MAJOR})
        string(REGEX REPLACE "^#define JSONRPC_CPP_MINOR_VERSION[ \t]+([0-9]+)" "\\1" jsonrpccpp_VERSION_MINOR ${jsonrpccpp_VERSION_MINOR})
        string(REGEX REPLACE "^#define JSONRPC_CPP_PATCH_VERSION[ \t]+([0-9]+)" "\\1" jsonrpccpp_VERSION_PATCH ${jsonrpccpp_VERSION_PATCH})
        set(jsonrpccpp_VERSION ${jsonrpccpp_VERSION_MAJOR}.${jsonrpccpp_VERSION_MINOR}.${jsonrpccpp_VERSION_PATCH})
    endif ()
endif ()

# handle the QUIETLY and REQUIRED arguments and set jsonrpccpp_FOUND to TRUE
# if all listed variables are TRUE, hide their existence from configuration view
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
        jsonrpccpp
        REQUIRED_VARS jsonrpccpp_INCLUDE_DIR jsonrpccpp_COMMON_LIBRARY jsonrpccpp_SERVER_LIBRARY jsonrpccpp_CLIENT_LIBRARY
        VERSION_VAR jsonrpccpp_VERSION
)

mark_as_advanced(jsonrpccpp_INCLUDE_DIR jsonrpccpp_COMMON_LIBRARY jsonrpccpp_SERVER_LIBRARY jsonrpccpp_CLIENT_LIBRARY)
