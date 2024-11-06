add_executable(taraxap main.cpp)
target_link_libraries(taraxap PRIVATE
    app
)

if(TCMALLOC_LIB)
    target_link_libraries(taraxap INTERFACE ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()

set(TARAXAP_INSTALL_DIR "/usr/local/bin" CACHE PATH "Taraxa application install directory")
install(PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/taraxap DESTINATION ${TARAXAP_INSTALL_DIR} COMPONENT taraxap)