set(prefix "${CMAKE_BINARY_DIR}/deps")
set(lib_path "${prefix}/lib")
set(include_path "${prefix}/include")

ExternalProject_Add(jsonrpccpp
    PREFIX "${prefix}"
    GIT_REPOSITORY https://github.com/MatusKysel/libjson-rpc-cpp.git
    GIT_SHALLOW true
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_CXX_COMPILER_LAUNCHER=${DCMAKE_CXX_COMPILER_LAUNCHER}
        -DJSONCPP_INCLUDE_DIR=${JSONCPP_INCLUDE_DIR}
        -DBUILD_STATIC_LIBS=1
        # disable build of parts that we don't need
        -DBUILD_SHARED_LIBS=0
        -DREDIS_SERVER=0
        -DREDIS_CLIENT=0
        -DCOMPILE_TESTS=0
        -DCOMPILE_STUBGEN=0
        -DCOMPILE_EXAMPLES=0
        -DWITH_COVERAGE=0
        -DHTTP_CLIENT=0
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
    INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target install
    BUILD_BYPRODUCTS "${lib_path}"
    DOWNLOAD_EXTRACT_TIMESTAMP NEW
    LOG_CONFIGURE 0
    LOG_BUILD 0
    LOG_INSTALL 0
)


add_library(Jsonrpccpp-common STATIC IMPORTED)
set_property(TARGET Jsonrpccpp-common PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET Jsonrpccpp-common PROPERTY IMPORTED_LOCATION "${lib_path}/${CMAKE_STATIC_LIBRARY_PREFIX}jsonrpccpp-common${CMAKE_STATIC_LIBRARY_SUFFIX}")
set_property(TARGET Jsonrpccpp-common PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${include_path})
add_dependencies(Jsonrpccpp-common jsonrpccpp)

add_library(Jsonrpccpp-server STATIC IMPORTED)
set_property(TARGET Jsonrpccpp-server PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET Jsonrpccpp-server PROPERTY IMPORTED_LOCATION  "${lib_path}/${CMAKE_STATIC_LIBRARY_PREFIX}jsonrpccpp-server${CMAKE_STATIC_LIBRARY_SUFFIX}")
set_property(TARGET Jsonrpccpp-server PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${include_path})
add_dependencies(Jsonrpccpp-server jsonrpccpp)

add_library(Jsonrpccpp-client STATIC IMPORTED)
set_property(TARGET Jsonrpccpp-client PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET Jsonrpccpp-client PROPERTY IMPORTED_LOCATION  "${lib_path}/${CMAKE_STATIC_LIBRARY_PREFIX}jsonrpccpp-client${CMAKE_STATIC_LIBRARY_SUFFIX}")
set_property(TARGET Jsonrpccpp-client PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${include_path})
add_dependencies(Jsonrpccpp-client jsonrpccpp)

add_library(Jsonrpccpp INTERFACE)
target_link_libraries(Jsonrpccpp INTERFACE Jsonrpccpp-common Jsonrpccpp-server Jsonrpccpp-client)
