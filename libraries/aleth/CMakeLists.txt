### Aleth part
include(ProjectSecp256k1)
include(ProjectLibFF)

add_subdirectory(libdevcore)
add_subdirectory(libdevcrypto)
add_subdirectory(libp2p)

add_library(aleth INTERFACE)

target_link_libraries(aleth INTERFACE devcore devcrypto p2p)

install(TARGETS devcore devcrypto p2p
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
