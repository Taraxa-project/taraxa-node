add_executable(taraxad main.cpp)
target_link_libraries(taraxad PRIVATE 
    cli
    common
    config
    consensus
    final_chain
    logger
    network
    node 
    storage
    transaction_manager
    aleth
    taraxa-evm
    taraxa-vrf
    vdf
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
    CONAN_PKG::boost
)

set(TARAXAD_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa application install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxad DESTINATION ${TARAXAD_INSTALL_DIR} COMPONENT taraxad)