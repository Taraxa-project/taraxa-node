add_executable(taraxa-bootnode main.cpp)
target_link_libraries(taraxa-bootnode PRIVATE
    cli
    p2p
)

set(TARAXA_BOOTNODE_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa bootnode install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxa-bootnode DESTINATION ${TARAXA_BOOTNODE_INSTALL_DIR} COMPONENT taraxa-bootnode)