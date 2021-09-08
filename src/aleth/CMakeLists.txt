### Aleth part
include(ProjectSecp256k1)
include(ProjectLibFF)

add_subdirectory(libdevcore)
add_subdirectory(libdevcrypto)
add_subdirectory(libp2p)

target_include_directories(devcore PUBLIC "./")
