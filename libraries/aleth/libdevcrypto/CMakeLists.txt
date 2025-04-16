file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(devcrypto ${SOURCES} ${HEADERS})

target_compile_options(devcrypto PRIVATE -Wno-extra-semi)
target_include_directories(devcrypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(cryptopp REQUIRED)
target_link_libraries(devcrypto PUBLIC devcore Secp256k1 libff::ff cryptopp::cryptopp gmp::gmp)
