add_executable(taraxad main.cpp)
target_link_libraries(taraxad PRIVATE 
    cli
    common
    config
    consensus
    logger
    network
    node 
    storage
)

if(TCMALLOC_LIB)
    target_link_libraries(taraxad INTERFACE ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()

set(TARAXAD_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa application install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxad DESTINATION ${TARAXAD_INSTALL_DIR} COMPONENT taraxad)