add_executable(taraxad main.cpp)
target_link_libraries(taraxad PRIVATE app_base)

set(TARAXAD_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa application install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxad DESTINATION ${TARAXAD_INSTALL_DIR} COMPONENT taraxad)