add_executable(taraxa-bootnode main.cpp)
target_link_libraries(taraxa-bootnode PRIVATE
    cli
    config
    common
    p2p
    devcore
)

set(TARAXAD_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa application install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxa-bootnode DESTINATION ${TARAXAD_INSTALL_DIR} COMPONENT taraxa-bootnode)