set(prefix "${CMAKE_BINARY_DIR}/deps")
set(SECP256K1_LIBRARY "${prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}secp256k1${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SECP256K1_INCLUDE_DIR "${prefix}/include")

ExternalProject_Add(secp256k1
    PREFIX "${prefix}"
    GIT_REPOSITORY https://github.com/Taraxa-project/secp256k1.git
    GIT_SHALLOW true
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_COMPILER_LAUNCHER=${DCMAKE_CXX_COMPILER_LAUNCHER}
        -DBUILD_SHARED_LIBS=0
        -DSECP256K1_ENABLE_MODULE_RECOVERY=ON
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
    INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target install
    BUILD_BYPRODUCTS "${lib_path}"
    DOWNLOAD_EXTRACT_TIMESTAMP NEW
    LOG_CONFIGURE 0
    LOG_BUILD 0
    LOG_INSTALL 0
)

# Create imported library
add_library(Secp256k1 STATIC IMPORTED)
file(MAKE_DIRECTORY "${SECP256K1_INCLUDE_DIR}")  # Must exist.
set_property(TARGET Secp256k1 PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET Secp256k1 PROPERTY IMPORTED_LOCATION_RELEASE "${SECP256K1_LIBRARY}")
set_property(TARGET Secp256k1 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SECP256K1_INCLUDE_DIR}")
add_dependencies(Secp256k1 secp256k1)