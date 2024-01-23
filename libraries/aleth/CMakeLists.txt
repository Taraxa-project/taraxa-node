### Aleth part
include(ProjectLibFF)

add_subdirectory(libsecp256k1)
add_subdirectory(libdevcore)
add_subdirectory(libdevcrypto)
add_subdirectory(libp2p)

install(TARGETS devcore devcrypto p2p secp256k1
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
