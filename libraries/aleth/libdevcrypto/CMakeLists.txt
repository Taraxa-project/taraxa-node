file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(devcrypto ${SOURCES} ${HEADERS})
target_include_directories(devcrypto PUBLIC ${UTILS_INCLUDE_DIR} ${GMP_INCLUDES})

target_link_libraries(devcrypto PUBLIC devcore Secp256k1 libff::ff CONAN_PKG::cryptopp ${GMP_LIBRARIES})
