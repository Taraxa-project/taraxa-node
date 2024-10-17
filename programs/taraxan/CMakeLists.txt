add_executable(taraxan main.cpp)
target_link_libraries(taraxan PRIVATE
    core_libs
)

if(TCMALLOC_LIB)
    target_link_libraries(taraxan INTERFACE ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()

set(TARAXAN_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa application install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxan DESTINATION ${TARAXAN_INSTALL_DIR} COMPONENT taraxan)