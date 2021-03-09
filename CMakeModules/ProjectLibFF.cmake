# Aleth: Ethereum C++ client, tools and libraries.
# Copyright 2017-2019 Aleth Authors.
# Licensed under the GNU General Public License, Version 3.
set(prefix "${CMAKE_BINARY_DIR}/deps")
set(libff_library "${prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ff${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(libff_inlcude_dir "${prefix}/include/libff")

file(GLOB gmp_libs "${CONAN_LIB_DIRS_GMP}/*gmp.*")

ExternalProject_Add(libff
    PREFIX "${prefix}"
    DOWNLOAD_NAME libff-03b719a7.tar.gz
    DOWNLOAD_NO_PROGRESS TRUE
    URL https://github.com/scipr-lab/libff/archive/03b719a7c81757071f99fc60be1f7f7694e51390.tar.gz
    URL_HASH SHA256=81b476089af43025c8f253cb1a9b5038a1c375baccffea402fa82042e608ab02
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DGMP_INCLUDE_DIR=${CONAN_INCLUDE_DIRS_GMP}
        -DGMP_LIBRARY=${gmp_libs}
        -DCURVE=ALT_BN128 -DPERFORMANCE=Off -DWITH_PROCPS=Off
        -DUSE_PT_COMPRESSION=Off
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
    LOG_BUILD 1
    INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target install
    BUILD_BYPRODUCTS "${libff_library}"
)

# Create snark imported library
add_library(libff::ff STATIC IMPORTED)
file(MAKE_DIRECTORY ${libff_inlcude_dir})
set_property(TARGET libff::ff PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET libff::ff PROPERTY IMPORTED_LOCATION_RELEASE ${libff_library})
set_property(TARGET libff::ff PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libff_inlcude_dir})
set_property(TARGET libff::ff PROPERTY INTERFACE_LINK_LIBRARIES CONAN_PKG::mpir)
add_dependencies(libff::ff libff)
